/*
 *
 * Copyright (c) 2013,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Notifications and licenses are retained for attribution purposes only
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010 Atheros Communications Inc.
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
 */ 

/*
*  Vendor generic IOCTL
*/
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
    #include <linux/config.h>
#endif
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>       /* XXX for ARPHRD_ETHER */
#include <net/iw_handler.h>

#include <asm/uaccess.h>

#include "if_media.h"
#include "_ieee80211.h"
#include <osif_private.h>
#include <wlan_opts.h>
#include <ieee80211_var.h>
#include "ieee80211_rateset.h"
#include "if_athvar.h"
#include "ah.h"
#include "version.h"
#ifdef ATH_USB
#include <osdep.h>
#include <linux/usb.h>
#endif

#include "ioctl_vendor_generic.h"      /* Vendor Include */
#include "ar9300/ar9300reg.h"

#if 1 /* Aqulia : support Multiple-Scan/more layering CWM. */
#define _CWM_GET_WIDTH(_ic)                                 (_ic)->ic_cwm_get_width((_ic))
#define _WLAN_SCAN_START(_vap, _scan_params, _osvsp)        \
    wlan_ucfg_scan_start((_vap), (_scan_params), _osvsp->vendor_scan_requestor, IEEE80211_SCAN_PRIORITY_LOW, &((_osvsp)->vendor_scan_id), 0, NULL)
#define _WLAN_SCAN_CANCEL(_vap, _osvsp)                     \
    wlan_ucfg_scan_cancel((_vap), (_osvsp)->scan_requestor, 0, WLAN_SCAN_CANCEL_VDEV_ALL, false)
#define _WLAN_SCAN_TABLE_ITERATE(_handle, _cb, _req)        \
    ucfg_scan_db_iterate((_handle), (_cb), (_req))
#else /* p2p_sag. */
#define _CWM_GET_WIDTH(_ic)                                 (_ic)->ic_cwm.cw_width
#define _WLAN_SCAN_START(_vap, _scan_params, _osvsp)        \
    wlan_ucfg_scan_start((_vap), (_scan_params), (_osvsp)->vendor_scan_id)
#define _WLAN_SCAN_CANCEL(_vap, _osvsp)                     \
    wlan_scan_cancel((_vap), (_osvsp)->vendor_scan_id, true)
#define _WLAN_SCAN_TABLE_ITERATE(_handle, _cb, _req)        \
    ucfg_scan_db_iterate(wlan_vap_get_devhandle(_handle), (_cb), (_req))
#endif

#ifdef ATH_DEBUG
#define IOCTL_LOG_PREFIX        "[ioctl_vendor]"
#define IOCTL_DPRINTF(_fmt, ...) do { printk(_fmt, __VA_ARGS__); } while (0)
#else
#define IOCTL_DPRINTF(_vap, _m, _fmt, ...)
#endif /* ATH_DEBUG */

#define IS_UP(_dev) \
    (((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))

struct ath_interface_softc {
    struct ath_softc_net80211   aps_sc;
    struct _NIC_DEV             aps_osdev;
};

#define IEEE80211_SCAN_ALLSIZE	64
struct _scan_result_all {
    u_int8_t                    cnt;
    athcfg_wcmd_scan_result_t   result[IEEE80211_SCAN_ALLSIZE];
};

/* Size of this structure must be lower than OSIF_VENDOR_RAWDATA_SIZE. */
struct _vendor_generic_ioctl {
    u_int32_t   tm_is_rx                :1,
                tm_is_up                :1,
                tm_is_bssid_set         :1,
                tm_is_chan_set          :1,
                tm_ant_sel              :2,         /* 0 menas not K2 or K2 w/ rx-diversity-on */
                scan_is_set             :1;
    u_int8_t    tm_channel;
    u_int8_t    tm_bssid[IEEE80211_ADDR_LEN];
    u_int8_t    tm_latestRssiCombined;
    u_int8_t    tm_latestRssiCtl[IEEE80211_MAX_ANTENNA];
    u_int8_t    tm_latestRssiExt[IEEE80211_MAX_ANTENNA];
    int         (*origional_wlan_receive_filter_80211) (os_if_t osif, wbuf_t wbuf,
                                       u_int16_t type, u_int16_t subtype,
                                       ieee80211_recv_status *rs);    
    systick_t   lastScanStartTime;
    systick_t   lastScanTime;
    wlan_scan_requester vendor_scan_requestor;
    wlan_scan_id vendor_scan_id;
};

#define IFREQ_TO_VENDOR_CMD(_iiReq)         ((athcfg_wcmd_t *)(_iiReq))
#define RAWDATA_TO_VENDOR_PRIV(_osRaw)      ((struct _vendor_generic_ioctl *)(_osRaw))

//////////////////////////////////////////////////////////////////////////////////////////////
static int _vendor_ioctl_getparam_versinfo(struct net_device * dev, athcfg_wcmd_data_t * iiData)
{
    char *dev_info = "ath_usb";
    int retv = 0;

    strncpy(iiData->version_info.version, ATH_DRIVER_VERSION, strlen(ATH_DRIVER_VERSION));
    strncpy(iiData->version_info.fw_version, ATH_DRIVER_VERSION, strlen(ATH_DRIVER_VERSION));
    strncpy(iiData->version_info.driver, dev_info, strlen(dev_info));

    IOCTL_DPRINTF("%s: ATH_IOCTL_VERSINFO - DRV [%s] VER [%s] FW [%s]\n", IOCTL_LOG_PREFIX,
            iiData->version_info.driver,
            iiData->version_info.version,
            iiData->version_info.fw_version);

    return retv;    
}

static int _vendor_ioctl_getparam_prodinfo(struct net_device * dev, athcfg_wcmd_data_t * iiData)
{
    int retv = 0;

#ifdef ATH_USB
    {
        struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
        
        struct ath_interface_softc *ais = (struct ath_interface_softc *)scn;
        struct usb_device *udev = ais->aps_osdev.udev;

        iiData->product_info.idProduct = le16_to_cpu(udev->descriptor.idProduct);
        iiData->product_info.idVendor = le16_to_cpu(udev->descriptor.idVendor);

        if (udev->manufacturer)
            strncpy(iiData->product_info.manufacturer, udev->manufacturer, strlen(udev->manufacturer));
        if (udev->product)
            strncpy(iiData->product_info.product, udev->product, strlen(udev->product));
        if (udev->serial)
            strncpy(iiData->product_info.serial, udev->serial, strlen(udev->serial));

        IOCTL_DPRINTF("%s: ATH_IOCTL_PRODINFO - VID 0x%02x PID 0x%02x PROD [%s] MANU [%s] SERL [%s]\n", IOCTL_LOG_PREFIX,
                iiData->product_info.idProduct,
                iiData->product_info.idVendor,
                iiData->product_info.product,
                iiData->product_info.manufacturer,
                iiData->product_info.serial);
    }
#else
    retv = -EOPNOTSUPP;
#endif    

    return retv;    
}

static int _vendor_ioctl_getparam_reg(struct net_device * dev, athcfg_wcmd_data_t * iiData)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ath_softc          *sc  = ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah  = sc->sc_ah;
    u_int32_t resultSize, tgtAddr;
    void *regBase, *regValue;
    int retv = 0;

    resultSize = sizeof(u_int32_t);
    
    regBase = (void *)OS_MALLOC(sc->sc_osdev, resultSize, GFP_KERNEL);
    regValue = (void *)OS_MALLOC(sc->sc_osdev, resultSize, GFP_KERNEL);

//    if (ath_hal_getdiagstate(ah, HAL_DIAG_GET_REGBASE, NULL, 0,
//                        &regBase, &resultSize) == AH_TRUE)
    {
//        tgtAddr = iiData->reg.addr + *((u_int32_t *)regBase);
        tgtAddr = iiData->reg.addr;
        if (ath_hal_getdiagstate(ah, HAL_DIAG_REGREAD, (void *)&tgtAddr, sizeof(u_int32_t),
                            &regValue, &resultSize) == AH_TRUE)
        {
            iiData->reg.val = *((u_int32_t *)regValue);
            IOCTL_DPRINTF("%s: ATH_IOCTL_REGRW - Read TgtAddr[%x] Val[%x]\n", IOCTL_LOG_PREFIX,
                            tgtAddr, iiData->reg.val);
        }
        else
            retv = -EIO;
    }
//    else
//        retv = -EIO;
    
    OS_FREE(regBase);
    OS_FREE(regValue);

    return retv;    
}

static int _vendor_ioctl_setparam_reg(struct net_device * dev, athcfg_wcmd_data_t * iiData)
{
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ath_softc          *sc  = ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah  = sc->sc_ah;
    u_int32_t resultSize, tgtAddr;
    void *regBase;
    int retv = 0;

    resultSize = sizeof(u_int32_t);
    regBase = (void *)OS_MALLOC(sc->sc_osdev, resultSize, GFP_KERNEL);

//    if (ath_hal_getdiagstate(ah, HAL_DIAG_GET_REGBASE, NULL, 0,
//                        &regBase, &resultSize) == AH_TRUE)
    {
//        tgtAddr = iiData->reg.addr + *((u_int32_t *)regBase);
        tgtAddr = iiData->reg.addr;
        if (ath_hal_getdiagstate(ah, HAL_DIAG_REGWRITE, (void *)&iiData->reg, sizeof(iiData->reg),
                            NULL, 0) == AH_TRUE)
        {
            IOCTL_DPRINTF("%s: ATH_IOCTL_REGRW - Write TgtAddr[%x] Val[%x]\n", IOCTL_LOG_PREFIX,
                            tgtAddr, iiData->reg.val);
        }
        else
            retv = -EIO;
    }
//    else
//        retv = -EIO;
    
    OS_FREE(regBase);
    
    return retv;    
}

static int _vendor_ioctl_getparam_txpower(struct net_device * dev, athcfg_wcmd_data_t * iiData)
{
#define AR_PHY_POWER_TX_RATE1   0x9934
#define AR_PHY_POWER_TX_RATE2   0x9938
#define AR_PHY_POWER_TX_RATE3   0xA234
#define AR_PHY_POWER_TX_RATE4   0xA238
#define AR_PHY_POWER_TX_RATE5   0xA38C
#define AR_PHY_POWER_TX_RATE6   0xA390
#define AR_PHY_POWER_TX_RATE7   0xA3CC
#define AR_PHY_POWER_TX_RATE8   0xA3D0
    struct ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ath_softc          *sc  = ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah  = sc->sc_ah;
    char *pRatesPower = iiData->txpower.txpowertable;
    int i, retv = 0;

    if (OS_REG_READ(ah, AR_PHY_POWER_TX_RATE1) != 0xdeadbeef)
    {   /* Merlin/K2 all tx power must -5 dBm in fact and the unit is 0.5 dBm. */
        for (i = 0; i < 4; i++)
            pRatesPower[i] = (u_int8_t)(OS_REG_READ(ah, AR_PHY_POWER_TX_RATE1) >> (8*i)) - 10;
        for (i = 4; i < 8; i++)
            pRatesPower[i] = (u_int8_t)(OS_REG_READ(ah, AR_PHY_POWER_TX_RATE2) >> (8*i)) - 10;
        for (i = 8; i < 12; i++)
            pRatesPower[i] = (u_int8_t)(OS_REG_READ(ah, AR_PHY_POWER_TX_RATE3) >> (8*i)) - 10;
        for (i = 12; i < 16; i++)
            pRatesPower[i] = (u_int8_t)(OS_REG_READ(ah, AR_PHY_POWER_TX_RATE4) >> (8*i)) - 10;
        for (i = 16; i < 20; i++)
            pRatesPower[i] = (u_int8_t)(OS_REG_READ(ah, AR_PHY_POWER_TX_RATE5) >> (8*i)) - 10;
        for (i = 20; i < 24; i++)        
            pRatesPower[i] = (u_int8_t)(OS_REG_READ(ah, AR_PHY_POWER_TX_RATE6) >> (8*i)) - 10;
        for (i = 24; i < 28; i++)
            pRatesPower[i] = (u_int8_t)(OS_REG_READ(ah, AR_PHY_POWER_TX_RATE7) >> (8*i)) - 10;
        for (i = 28; i < 32; i++)
            pRatesPower[i] = (u_int8_t)(OS_REG_READ(ah, AR_PHY_POWER_TX_RATE8) >> (8*i)) - 10;
    }

    return retv;    
}

static int _vendor_ioctl_getparam_devStats(struct net_device *dev, athcfg_wcmd_data_t * iiData)
{
    return 0;    
}

static int ath_vendor_ioctl_setparam(struct net_device *dev, athcfg_wcmd_t *iiCmd)
{
    int retv = -EINVAL;

    switch (iiCmd->iic_cmd) 
    {
    case ATH_IOCTL_PRODINFO:
        /* no meaning... */
        retv = 0;
        break;

    case ATH_IOCTL_REGRW:
        retv = _vendor_ioctl_setparam_reg(dev, &iiCmd->iic_data);
        break;
        
    default:
        retv = -EOPNOTSUPP;
        break;
    }

    return retv;
}

static int ath_vendor_ioctl_getparam(struct net_device *dev, athcfg_wcmd_t *iiCmd)
{
    int retv = 0; 

    switch (iiCmd->iic_cmd) 
    {
    case ATH_IOCTL_VERSINFO:
        retv = _vendor_ioctl_getparam_versinfo(dev, &iiCmd->iic_data);
        break;

    case ATH_IOCTL_PRODINFO:
        retv = _vendor_ioctl_getparam_prodinfo(dev, &iiCmd->iic_data);
        break;

    case ATH_IOCTL_REGRW:
        retv = _vendor_ioctl_getparam_reg(dev, &iiCmd->iic_data);
        break;

    case ATH_IOCTL_TX_POWER:
        retv = _vendor_ioctl_getparam_txpower(dev, &iiCmd->iic_data);
        break;

    case ATH_IOCTL_STATS:
        retv = _vendor_ioctl_getparam_devStats(dev, &iiCmd->iic_data);
        break;
        
    default:
        retv = -EOPNOTSUPP;
        break;
    }
    
    return retv; 
}

static int ath_ioctl_vendor_generic(struct net_device *dev, ioctl_ifreq_req_t *iiReq)
{
    athcfg_wcmd_t *iiCmd, *in = IFREQ_TO_VENDOR_CMD(iiReq);
    int cmd, retv = -EOPNOTSUPP;

    iiCmd = qdf_mem_malloc(sizeof(athcfg_wcmd_t));
    __xcopy_from_user(iiCmd, in, sizeof(athcfg_wcmd_t));
    cmd = iiCmd->iic_cmd;

    IOCTL_DPRINTF("%s: %s - Vendor 0x%x CMD 0x%08x\n",IOCTL_LOG_PREFIX, __func__,
                    iiCmd->iic_vendor, iiCmd->iic_cmd);   
    
    if (iiCmd->iic_vendor != ATHCFG_WCMD_VENDORID)
        return retv;
    
    if (!(cmd & IOCTL_PHYDEV_MASK))
    {
        IOCTL_DPRINTF("%s: Physical Device doesn't supoort this CMD.\n", IOCTL_LOG_PREFIX);
            
        return retv;
    }

    if (cmd & IOCTL_SET_MASK)
    {
        iiCmd->iic_cmd &= ~IOCTL_SET_MASK;
        
        retv = ath_vendor_ioctl_setparam(dev, iiCmd);
    }
    else if (cmd & IOCTL_GET_MASK)    
    {
        iiCmd->iic_cmd &= ~IOCTL_GET_MASK;
        
        retv = ath_vendor_ioctl_getparam(dev, iiCmd);
    }

    if (retv == 0)
        _copy_to_user(in, iiCmd, sizeof(athcfg_wcmd_t));
    qdf_mem_free(iiCmd);

    return retv;
}
//////////////////////////////////////////////////////////////////////////////////////////////
static int _vendor_ioctl_getparam_phymode_is11bRates(struct ieee80211_rateset *rs)
{
#define N(a)	(sizeof(a) / sizeof(a[0]))
    static const int32_t rates[] = { 2, 4, 11, 22 };
    u_int8_t i, j;

    if (rs->rs_nrates > N(rates))
        return 0;

    for (i = 0; i < rs->rs_nrates; i++) 
    {
        int32_t r = rs->rs_rates[i] & IEEE80211_RATE_VAL;

        for (j = 0; j < N(rates); j++) 
        {
            if (r == rates[j])
                break;
        }
        if (j == N(rates))
            return 0;
    }
    
    return 1;
#undef N
}

static int _vendor_ioctl_getparam_phymode_isHTAllowed(struct ieee80211vap *vap)
{
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    struct ieee80211com *ic = vap->iv_ic;

    switch (vap->iv_cur_mode) 
    {
    case IEEE80211_MODE_11A:
    case IEEE80211_MODE_11B:
    case IEEE80211_MODE_11G:
    case IEEE80211_MODE_FH:
    case IEEE80211_MODE_TURBO_A:
    case IEEE80211_MODE_TURBO_G:
        return 0;

    default:
        break;
    }

    if (!ieee80211_ic_wep_tkip_htrate_is_set(ic) &&
        IEEE80211_VAP_IS_PRIVACY_ENABLED(vap) &&
        (RSN_CIPHER_IS_WEP(rsn) ||
          (RSN_CIPHER_IS_TKIP(rsn) 
           && !RSN_CIPHER_IS_CCMP128(rsn)
           && !RSN_CIPHER_IS_CCMP256(rsn)
           && !RSN_CIPHER_IS_GCMP128(rsn)
           && !RSN_CIPHER_IS_GCMP256(rsn))))
        return 0;
    else
        return 1;
}

static athcfg_wcmd_phymode_t _vendor_ioctl_getparam_phymode_get(struct net_device *dev)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ifmedia *media = &osifp->os_media;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni = vap->iv_bss;
    struct ifmediareq imr;
    athcfg_wcmd_phymode_t phymode = ATHCFG_WCMD_PHYMODE_AUTO; 
   
    if (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS)
    {
        media->ifm_status(dev, &imr);       /* get current media status. */

        switch(imr.ifm_active & 0x001f0000)
        {
        case IFM_IEEE80211_11A:
            phymode = ATHCFG_WCMD_PHYMODE_11A;
            break;
            
        case IFM_IEEE80211_11B:
            phymode = ATHCFG_WCMD_PHYMODE_11B;
            break;
            
        case IFM_IEEE80211_11G:
            if (_vendor_ioctl_getparam_phymode_is11bRates(&ni->ni_rates))
                phymode = ATHCFG_WCMD_PHYMODE_11B;
            else
                phymode = ATHCFG_WCMD_PHYMODE_11G;
            break;

        case IFM_IEEE80211_11NA:
        case (IFM_IEEE80211_11NA | IFM_IEEE80211_HT40PLUS):
        case (IFM_IEEE80211_11NA | IFM_IEEE80211_HT40MINUS):
            /* AP and STA are HT? */
            if ((ni->ni_flags & IEEE80211_NODE_HT) &&
                _vendor_ioctl_getparam_phymode_isHTAllowed(vap)) 
            {
                if (ni->ni_chwidth == IEEE80211_CWM_WIDTH40 && 
                   (_CWM_GET_WIDTH(ic) == IEEE80211_CWM_WIDTH40)) 
                    phymode = ATHCFG_WCMD_PHYMODE_11NA_HT40;
                else
                    phymode = ATHCFG_WCMD_PHYMODE_11NA_HT20;
            }
            else
                phymode = ATHCFG_WCMD_PHYMODE_11A;
            break;
            
        case IFM_IEEE80211_11NG:
        case (IFM_IEEE80211_11NG | IFM_IEEE80211_HT40PLUS):
        case (IFM_IEEE80211_11NG | IFM_IEEE80211_HT40MINUS):
            if ((ni->ni_flags & IEEE80211_NODE_HT) &&
                _vendor_ioctl_getparam_phymode_isHTAllowed(vap)) 
            {
                if (ni->ni_chwidth == IEEE80211_CWM_WIDTH40 &&
                   (_CWM_GET_WIDTH(ic) == IEEE80211_CWM_WIDTH40)) 
                    phymode = ATHCFG_WCMD_PHYMODE_11NG_HT40;
                else
                    phymode = ATHCFG_WCMD_PHYMODE_11NG_HT20;
            }
            else if (_vendor_ioctl_getparam_phymode_is11bRates(&ni->ni_rates))
                phymode = ATHCFG_WCMD_PHYMODE_11B;
            else
                phymode = ATHCFG_WCMD_PHYMODE_11G;
            break;
            
        default:
            break;
        }
    }
    
    return phymode;
}

static int _vendor_ioctl_getparam_phymode(struct net_device *dev, athcfg_wcmd_data_t *iiData)
{
    int retv = 0; 

    iiData->phymode = _vendor_ioctl_getparam_phymode_get(dev);

    IOCTL_DPRINTF("%s: IEEE80211_IOCTL_PHYMODE - PHYMODE %d\n", IOCTL_LOG_PREFIX,
                        iiData->phymode);
   
    return retv;
}

static int _vendor_ioctl_getparam_stainfo_isShortGI(struct net_device *dev)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni = vap->iv_bss;

    if ((wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) && (ni))
    {
        if (((ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI40) && (_CWM_GET_WIDTH(ic) == IEEE80211_CWM_WIDTH40)) ||
            ((ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI20) && (_CWM_GET_WIDTH(ic) == IEEE80211_CWM_WIDTH20))) 
            return 1;
        else
            return 0;
    }
    else
        return 0;
}
    
static int _vendor_ioctl_getparam_stainfo(struct net_device *dev, athcfg_wcmd_data_t *iiData)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211_node *ni = vap->iv_bss;
    wlan_rssi_info rssi_info;
    int retv = 0;      

    if ((wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) && (ni))
    {
        iiData->station_info.flags |= ATHCFG_WCMD_STAINFO_LINKUP;
        if (_vendor_ioctl_getparam_stainfo_isShortGI(dev))
            iiData->station_info.flags |= ATHCFG_WCMD_STAINFO_SHORTGI;
        iiData->station_info.phymode = _vendor_ioctl_getparam_phymode_get(dev);
        iiData->station_info.assoc_time = CONVERT_SYSTEM_TIME_TO_MS(ni->ni_assoctime);
        iiData->station_info.rx_rate_kbps = (ATH_NODE_NET80211(ni)->an_lastrxrate);
        if ((ATH_NODE_NET80211(ni)->an_rxratecode) & 0x80)
            iiData->station_info.rx_rate_mcs = (ATH_NODE_NET80211(ni)->an_rxratecode) & ~0x80;
        else
            iiData->station_info.rx_rate_mcs = ATHCFG_WCMD_STAINFO_MCS_NULL;
        OS_MEMCPY(iiData->station_info.bssid, ni->ni_bssid, ATHCFG_WCMD_ADDR_LEN);
        if (wlan_getrssi(vap, &rssi_info, WLAN_RSSI_RX) == -1)
            iiData->station_info.rx_rssi = rssi_info.avg_rssi;
        else
            iiData->station_info.rx_rssi = ni->ni_rssi; /* go back to 802.11 layer's */
        if (wlan_getrssi(vap, &rssi_info, WLAN_RSSI_BEACON) == -1)
            iiData->station_info.rx_rssi_beacon = rssi_info.avg_rssi;
        else
            iiData->station_info.rx_rssi_beacon = ni->ni_rssi; /* go back to 802.11 layer's */

#if 1 /* workaround for TGT FW report */
        if (iiData->station_info.rx_rate_kbps == 81500)
            iiData->station_info.rx_rate_kbps = 81000;
        if (iiData->station_info.rx_rate_kbps == 27500)
            iiData->station_info.rx_rate_kbps = 27000;        
        if (iiData->station_info.tx_rate_kbps == 81500)
            iiData->station_info.tx_rate_kbps = 81000;
        if (iiData->station_info.tx_rate_kbps == 27500)
            iiData->station_info.tx_rate_kbps = 27000;        
#endif
        
        IOCTL_DPRINTF("%s: IEEE80211_IOCTL_STAINFO - PHY %d ASSOTIME %d RX/TX RATE %d[%02x]/%d BSSID %02x:%02x:%02x:%02x:%02x:%02x RSSI %d %d %d\n", IOCTL_LOG_PREFIX,
                            iiData->station_info.phymode,
                            iiData->station_info.assoc_time,
                            iiData->station_info.rx_rate_kbps, (ATH_NODE_NET80211(ni)->an_rxratecode),
                            iiData->station_info.tx_rate_kbps,
                            iiData->station_info.bssid[0], iiData->station_info.bssid[1], iiData->station_info.bssid[2],
                            iiData->station_info.bssid[3], iiData->station_info.bssid[4], iiData->station_info.bssid[5],
                            iiData->station_info.rx_rssi,
                            iiData->station_info.rx_rssi_beacon,
                            ni->ni_rssi);
    }
    else
    {
        IOCTL_DPRINTF("%s: IEEE80211_IOCTL_STAINFO - No Connection exist! \n", IOCTL_LOG_PREFIX);
    }

    return retv;
}

static int _vendor_ioctl_setparam_isDot11Channel(struct net_device *dev, int ieee)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    int i;

    for (i = 0; i < ic->ic_nchans; i++) 
    {
        struct ieee80211_ath_channel *c = &ic->ic_channels[i];

        if (ieee && (c->ic_ieee == ieee))
            return 1;
    }
    return 0;
}

static int _vendor_ioctl_setparam_isAR9271(struct net_device *dev)
{
#define PRODUCT_AR9271          0x9271

#ifdef ATH_USB
    {
        osif_dev  *osifp = ath_netdev_priv(dev);
        struct net_device *parentDev = osifp->os_comdev;
        struct ath_softc_net80211 *scn = ath_netdev_priv(parentDev);
        struct ath_interface_softc *ais = (struct ath_interface_softc *)scn;
        struct usb_device *udev = ais->aps_osdev.udev;

        if (le16_to_cpu(udev->descriptor.idProduct) == PRODUCT_AR9271)
            return 1;
    }
#endif    

    return 0;    
}

static int _vendor_ioctl_setparam_testmode_receive_filter_80211(os_if_t osif, wbuf_t wbuf,
                                        u_int16_t type, u_int16_t subtype,
                                        ieee80211_recv_status *rs)
{
    osif_dev  *osifp = (osif_dev *) osif;
    struct _vendor_generic_ioctl *osvsp = RAWDATA_TO_VENDOR_PRIV(osifp->os_vendor_specific);
    struct ieee80211_frame *wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    int i;

    if ((osvsp->tm_is_rx) &&
        ((type == IEEE80211_FC0_TYPE_MGT) && (subtype == IEEE80211_FC0_SUBTYPE_BEACON)) &&
        (OS_MEMCMP(osvsp->tm_bssid, wh->i_addr2, IEEE80211_ADDR_LEN) == 0))
    {
        osvsp->tm_latestRssiCombined = rs->rs_rssi;
        for (i = 0; i < IEEE80211_MAX_ANTENNA; i++)
        {
            osvsp->tm_latestRssiCtl[i] = rs->rs_rssictl[i];
            osvsp->tm_latestRssiExt[i] = rs->rs_rssiextn[i];
        }

#if 0
        IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - GET RESULT, ANT %d, RX-BSSID %02x:%02x:%02x:%02x:%02x:%02x, RSSI %d\n", IOCTL_LOG_PREFIX,
                            osvsp->tm_ant_sel,
                            wh->i_addr2[0], wh->i_addr2[1], wh->i_addr2[2], 
                            wh->i_addr2[3], wh->i_addr2[4], wh->i_addr2[5],
                            osvsp->tm_latestRssiCombined);
#endif

        if (1)
        {   /* Solution for AR9271 */ 
            if (_vendor_ioctl_setparam_isAR9271(osifp->netdev))
            {
                if (osvsp->tm_ant_sel == 0)
                {
                    osvsp->tm_latestRssiCtl[1] = osvsp->tm_latestRssiCtl[0];
                    osvsp->tm_latestRssiCtl[2] = 0;
                    osvsp->tm_latestRssiExt[1] = osvsp->tm_latestRssiExt[0];
                    osvsp->tm_latestRssiExt[2] = 0;
                }
                else if (osvsp->tm_ant_sel == 1)
                {
                    osvsp->tm_latestRssiCtl[1] = 
                    osvsp->tm_latestRssiCtl[2] = 0;
                    osvsp->tm_latestRssiExt[1] = 
                    osvsp->tm_latestRssiExt[2] = 0;                    
                }
                else 
                {
                    osvsp->tm_latestRssiCtl[1] = osvsp->tm_latestRssiCtl[0];
                    osvsp->tm_latestRssiCtl[0] = 
                    osvsp->tm_latestRssiCtl[2] = 0;
                    osvsp->tm_latestRssiExt[1] = osvsp->tm_latestRssiExt[0];
                    osvsp->tm_latestRssiExt[0] = 
                    osvsp->tm_latestRssiExt[2] = 0;
                }
            }
        }
    }

    return -1; /* always reclaim it! */
}

static void _vendor_ioctl_setparam_testmode_antselection(struct net_device *dev)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    struct _vendor_generic_ioctl*osvsp = RAWDATA_TO_VENDOR_PRIV(osifp->os_vendor_specific);
    /* To Parent */
    struct net_device *parentDev = osifp->os_comdev;
    struct ath_softc_net80211 *scn = ath_netdev_priv(parentDev);
    struct ath_softc          *sc  = ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah  = sc->sc_ah;
    u_int32_t resultSize, tgtReg[2];
    void *regBase;

    if (_vendor_ioctl_setparam_isAR9271(dev))
    {   /* Solution for AR9271 : Turn-off RX Diversity and select RX Antenna for RSSI measurement. */ 
        resultSize = sizeof(u_int32_t);
        regBase = (void *)OS_MALLOC(sc->sc_osdev, resultSize, GFP_KERNEL);       
 //       ath_hal_getdiagstate(ah, HAL_DIAG_GET_REGBASE, NULL, 0,
 //                           &regBase, &resultSize);

//        tgtReg[0] = *((u_int32_t *)regBase) + (0x99ac);
        tgtReg[0] = (0x99ac);
        tgtReg[1] = (osvsp->tm_ant_sel == 0) ? (0x2def0400) : 
                                               ((osvsp->tm_ant_sel == 1) ? 0x52ef0400 : 0x2cef0400);
        ath_hal_getdiagstate(ah, HAL_DIAG_REGWRITE, (void *)&tgtReg, sizeof(tgtReg),
                                NULL, 0);

 //       tgtReg[0] = *((u_int32_t *)regBase) + (0xa208);
        tgtReg[0] = (0xa208);
        tgtReg[1] = (osvsp->tm_ant_sel == 0) ? (0x803e68c8) : (0x803e48c8);
        ath_hal_getdiagstate(ah, HAL_DIAG_REGWRITE, (void *)&tgtReg, sizeof(tgtReg),
                                NULL, 0);
        
        OS_FREE(regBase);
        
        IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - Change RX-ANT to %s\n", IOCTL_LOG_PREFIX,
                                 ((osvsp->tm_ant_sel == 0) ? "AUTO" : 
                                                             ((osvsp->tm_ant_sel == 1) ? "LNA1" : "LNA2")));
    }
    
    return;
}

static void _vendor_ioctl_setparam_testmode_rxfilter(struct net_device *dev)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    /* To Parent */
    struct net_device *parentDev = osifp->os_comdev;
    struct ath_softc_net80211 *scn = ath_netdev_priv(parentDev);
    struct ath_softc          *sc  = ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah  = sc->sc_ah;
    u_int32_t resultSize, tgtReg[2];
    void *regBase;
       
    resultSize = sizeof(u_int32_t);
    regBase = (void *)OS_MALLOC(sc->sc_osdev, resultSize, GFP_KERNEL);
    printk("%s:%d\n", __func__, __LINE__);
//    ath_hal_getdiagstate(ah, HAL_DIAG_GET_REGBASE, NULL, 0,
//                        &regBase, &resultSize);

//    tgtReg[0] = *((u_int32_t *)regBase) + AR_RX_FILTER /*AR_RX_FILTER*/;
    tgtReg[0] = AR_RX_FILTER /*AR_RX_FILTER*/;
    tgtReg[1] = (HAL_RX_FILTER_MYBEACON | HAL_RX_FILTER_PROM | HAL_RX_FILTER_MCAST);
    printk("%s:%d\n", __func__, __LINE__);
    ath_hal_getdiagstate(ah, HAL_DIAG_REGWRITE, (void *)&tgtReg, sizeof(tgtReg),
                            NULL, 0);
    printk("%s:%d\n", __func__, __LINE__);

    OS_FREE(regBase);
    
    IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - Go to listen BEACON...\n", IOCTL_LOG_PREFIX);
    
    return;
}

static void _vendor_ioctl_setparam_testmode_setPwrState(struct net_device *dev)
{
#ifdef UMAC_SUPPORT_RESMGR
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);

    ath_resume(scn);
    scn->sc_ops->awake(scn->sc_dev);

    IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - AWAKE\n", IOCTL_LOG_PREFIX);
#else
    /* Control by ath_dev. */
#endif

    return;
}

static int _vendor_ioctl_setparam_testmode(struct net_device *dev, athcfg_wcmd_data_t *iiData)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct _vendor_generic_ioctl *osvsp = RAWDATA_TO_VENDOR_PRIV(osifp->os_vendor_specific);
    int i, retv = -EINVAL;    

    switch (iiData->testmode.operation) 
    {
    case ATHCFG_WCMD_TESTMODE_CHAN:
        i = iiData->testmode.chan;
        if (((i > 0) && (i < IEEE80211_CHAN_MAX)) &&
            (_vendor_ioctl_setparam_isDot11Channel(dev, i)))
        {
            osvsp->tm_is_chan_set = 1;
            osvsp->tm_channel = i;

            IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - Chan %d\n", IOCTL_LOG_PREFIX, osvsp->tm_channel);
            
            retv = 0;                
        }           
        else
        {
            IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - Setting Channel fail1\n", IOCTL_LOG_PREFIX);
        }
        break;

    case ATHCFG_WCMD_TESTMODE_BSSID:
        osvsp->tm_is_bssid_set = 1;        
        for (i = 0; i < ATHCFG_WCMD_ADDR_LEN; i++)
            osvsp->tm_bssid[i] = iiData->testmode.bssid[i];
        
        IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - BSSID %02x:%02x:%02x:%02x:%02x:%02x\n", IOCTL_LOG_PREFIX, 
                        osvsp->tm_bssid[0],osvsp->tm_bssid[1],osvsp->tm_bssid[2],
                        osvsp->tm_bssid[3],osvsp->tm_bssid[4],osvsp->tm_bssid[5]);

        retv = 0;  
        break;
        
    case ATHCFG_WCMD_TESTMODE_RX:
        if (osvsp->tm_is_chan_set &&
            osvsp->tm_is_bssid_set) 
        {
            if (iiData->testmode.rx)
            {
                if (!osvsp->tm_is_rx)
                {
                    /* Cache current status */
                    osvsp->tm_is_up = IS_UP(dev);
                    
                    /* Stop current connection if have. */
                    if (wlan_connection_sm_stop(osifp->sm_handle, IEEE80211_CONNECTION_SM_STOP_ASYNC) == 0) 
                    {
                        i = 0;
                        while(osifp->is_up && i < 3) 
                        {
                            schedule_timeout_interruptible(HZ);
                            i++;
                        }
                        if (osifp->is_up) 
                        {
                            IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - Stop Connection fail?\n", IOCTL_LOG_PREFIX);
                            return -EBUSY;
                        }
                    }

                    /* Setup VAP & replace event call-back function. */
                    osvsp->origional_wlan_receive_filter_80211 = vap->iv_evtable->wlan_receive_filter_80211;
                    vap->iv_evtable->wlan_receive_filter_80211 = _vendor_ioctl_setparam_testmode_receive_filter_80211;
                    osifp->os_comdev->flags |= IFF_PROMISC;
                    _vendor_ioctl_setparam_testmode_setPwrState(dev);       /* Wake-up the chip */
                    vap->iv_listen(vap);
                    printk("%s:%d\n", __func__, __LINE__);

                    /* Change channgel to Test Channel. */
                    if (wlan_set_channel(vap, osvsp->tm_channel, vap->iv_des_cfreq2))
                    {
                        /* Rollback event call-back function & Restart the state machine. */
                        vap->iv_evtable->wlan_receive_filter_80211 = osvsp->origional_wlan_receive_filter_80211;
                        osvsp->origional_wlan_receive_filter_80211 = NULL;
                        osifp->os_comdev->flags &= ~IFF_PROMISC;
                        if (osvsp->tm_is_up)
                            wlan_connection_sm_start(osifp->sm_handle);
                        
                        IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - Change Channel fail?\n", IOCTL_LOG_PREFIX);
                        return -EBUSY;
                    }
                    printk("%s:%d\n", __func__, __LINE__);

                    /* RX Filter */
                    _vendor_ioctl_setparam_testmode_rxfilter(dev);

                    /* Ant. selection for K2 solution. */
                    if (_vendor_ioctl_setparam_isAR9271(dev))
                        _vendor_ioctl_setparam_testmode_antselection(dev);

                    /* Clear buffer */
                    osvsp->tm_latestRssiCombined = 0;
                    OS_MEMSET(osvsp->tm_latestRssiCtl, 0, IEEE80211_MAX_ANTENNA);
                    OS_MEMSET(osvsp->tm_latestRssiExt, 0, IEEE80211_MAX_ANTENNA);

                    /* Start Measurement */
                    osvsp->tm_is_rx = 1;

                    IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - START, Chan %d, BSSID %02x:%02x:%02x:%02x:%02x:%02x\n", IOCTL_LOG_PREFIX,
                                        osvsp->tm_channel, 
                                        osvsp->tm_bssid[0], osvsp->tm_bssid[1], osvsp->tm_bssid[2], 
                                        osvsp->tm_bssid[3], osvsp->tm_bssid[4], osvsp->tm_bssid[5]);
                }
            }
            else 
            {
                if (osvsp->tm_is_rx)
                {
                    /* Stop Measurement */
                    osvsp->tm_is_rx = 0;

                    /* Clear buffer */
                    osvsp->tm_latestRssiCombined = 0;
                    OS_MEMSET(osvsp->tm_latestRssiCtl, 0, IEEE80211_MAX_ANTENNA);
                    OS_MEMSET(osvsp->tm_latestRssiExt, 0, IEEE80211_MAX_ANTENNA);

                    /* Rollback event call-back function & Restart the state machine. */
                    vap->iv_evtable->wlan_receive_filter_80211 = osvsp->origional_wlan_receive_filter_80211;
                    osvsp->origional_wlan_receive_filter_80211 = NULL;
                    osifp->os_comdev->flags &= ~IFF_PROMISC;
                    if (osvsp->tm_is_up)
                        wlan_connection_sm_start(osifp->sm_handle);

                    IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - STOP\n", IOCTL_LOG_PREFIX);
                }
            }

            retv = 0; 
        }
        else
        {
            IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - Need setup Channel/BSSID and bring-up the device first !\n", IOCTL_LOG_PREFIX);
        }
        break;
        
    case ATHCFG_WCMD_TESTMODE_RESULT:
        /* no meaning... */
        IOCTL_DPRINTF("%s: IEEE80211_IOCTL_TESTMODE - No meaning...\n", IOCTL_LOG_PREFIX);
        break;
        
    case ATHCFG_WCMD_TESTMODE_ANT:
        if ((_vendor_ioctl_setparam_isAR9271(dev)) &&
            (iiData->testmode.antenna < IEEE80211_MAX_ANTENNA))
        {
            osvsp->tm_ant_sel = iiData->testmode.antenna;
            if (osvsp->tm_is_rx)
                _vendor_ioctl_setparam_testmode_antselection(dev);
        }
        retv = 0;                
        break;
        
    default:
        retv = -EOPNOTSUPP;
        break;
    }

    return retv;
}

static int _vendor_ioctl_getparam_testmode(struct net_device *dev, athcfg_wcmd_data_t *iiData)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    struct _vendor_generic_ioctl *osvsp = RAWDATA_TO_VENDOR_PRIV(osifp->os_vendor_specific);
    int retv = 0;    

    switch (iiData->testmode.operation) 
    {
    case ATHCFG_WCMD_TESTMODE_CHAN:
        if (osvsp->tm_is_chan_set)
            iiData->testmode.chan = osvsp->tm_channel;             
        break;
        
    case ATHCFG_WCMD_TESTMODE_BSSID:
        if (osvsp->tm_is_bssid_set)
             OS_MEMCPY(iiData->testmode.bssid, osvsp->tm_bssid, ATHCFG_WCMD_ADDR_LEN);
        break;
         
    case ATHCFG_WCMD_TESTMODE_RX:
        iiData->testmode.rx = osvsp->tm_is_rx;
        break;  
        
    case ATHCFG_WCMD_TESTMODE_RESULT:
        iiData->testmode.rssi_combined = osvsp->tm_latestRssiCombined;
        iiData->testmode.rssi0 = osvsp->tm_latestRssiCtl[0];
        iiData->testmode.rssi1 = osvsp->tm_latestRssiCtl[1];
        iiData->testmode.rssi2 = osvsp->tm_latestRssiCtl[2];
        break; 
        
    case ATHCFG_WCMD_TESTMODE_ANT:
        iiData->testmode.antenna = osvsp->tm_ant_sel;
        break;
        
    default:
        retv = -EOPNOTSUPP;
        break;
    }

    return retv;
}

static void _vendor_ioctl_getparam_scantime_evhandler(wlan_if_t vaphandle, struct scan_event *event, void *arg)
{
    osif_dev  *osifp = (osif_dev *)arg;
    struct _vendor_generic_ioctl *osvsp = RAWDATA_TO_VENDOR_PRIV(osifp->os_vendor_specific);

    if ((osvsp->scan_is_set) &&
        (event->type == SCAN_EVENT_TYPE_COMPLETED) &&
        (event->reason != SCAN_REASON_CANCELLED))
    {
        if (osvsp->lastScanStartTime)
        {
            osvsp->lastScanTime = OS_GET_TICKS() - osvsp->lastScanStartTime;
            osvsp->lastScanStartTime = 0;
            osvsp->scan_is_set = 0;
        }
    }

    return;
}

static int _vendor_ioctl_getparam_scantime(struct net_device *dev, athcfg_wcmd_data_t *iiData)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct _vendor_generic_ioctl *osvsp = RAWDATA_TO_VENDOR_PRIV(osifp->os_vendor_specific);
    struct scan_start_request *scan_params = NULL;
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    int retv = -EINVAL;
    int loop = 0;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_vdev *vdev = NULL;

    if (dev->flags & IFF_UP)
    {
        if (osvsp->scan_is_set)
        {
            IOCTL_DPRINTF("%s: IEEE80211_IOCTL_SCANTIME - scan already going...\n", IOCTL_LOG_PREFIX);

            return retv;
        }
        else
            osvsp->scan_is_set = 1;

        vdev = osifp->ctrl_vdev;
        status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);
        if (QDF_IS_STATUS_ERROR(status)) {
            osvsp->scan_is_set = 0;
            scan_info("unable to get reference");
            return -EBUSY;
        }

        psoc = wlan_vap_get_psoc(vap);

        /* we are here means vendor scan is not in progress.
         * since this scan can go without interrupting any other scan,
         * as it has got its own scan id and requester id, go ahead and
         * start a new vendor scan. No need to cancel any other scan.
         *
         * if (wlan_vdev_scan_in_progress(vap->vdev_obj))
         *   _WLAN_SCAN_CANCEL(vap, osvsp);
         */
        scan_params = (struct scan_start_request*) qdf_mem_malloc(sizeof(*scan_params));
        if (!scan_params) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            osvsp->scan_is_set = 0;
            return -ENOMEM;
        }

        status = wlan_update_scan_params(vap,scan_params,osifp->os_opmode,
                true,true,true,true,0,NULL,1);
        if (status) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            qdf_mem_free(scan_params);
            osvsp->scan_is_set = 0;
            scan_err("scan param init failed with status: %d", status);
            return -EINVAL;
        }

        scan_params->legacy_params.min_dwell_active =
            scan_params->scan_req.dwell_time_active = 100;
        scan_params->legacy_params.scan_type = SCAN_TYPE_FOREGROUND;
        scan_params->scan_req.scan_flags = 0;
        scan_params->scan_req.scan_f_passive = false;
        scan_params->scan_req.scan_f_2ghz = true;
        scan_params->scan_req.scan_f_5ghz = true;

        /* Calc. SCAN time */
        osvsp->vendor_scan_requestor =
            ucfg_scan_register_requester(psoc, (uint8_t*)"vendor_scantime",
                    _vendor_ioctl_getparam_scantime_evhandler, (void*)osifp);

        if (!osvsp->vendor_scan_requestor) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            scan_err("unable to allocate requester");
            qdf_mem_free(scan_params);
            osvsp->scan_is_set = 0;
            return -ENOMEM;
        }
        osvsp->lastScanStartTime = OS_GET_TICKS();

        /* Scan request */
        status = _WLAN_SCAN_START(vap, scan_params, osvsp);
        if (status) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            scan_err("scan_start failed with status: %d", status);
            osvsp->scan_is_set = 0;
            return -EBUSY;
        }

        /* Get Scan Result */
        while (osvsp->scan_is_set)
        {
            schedule_timeout_interruptible(3*HZ);

            if (++loop == 6)
            {
                ucfg_scan_unregister_requester(psoc, osvsp->vendor_scan_requestor);
                wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
                osvsp->lastScanStartTime = 0;
                osvsp->scan_is_set = 0;
                osvsp->vendor_scan_requestor = 0;
                osvsp->vendor_scan_id = 0;
                return -EIO;
            }
        }
        ucfg_scan_unregister_requester(psoc, osvsp->vendor_scan_requestor);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);

        iiData->scantime.scan_time = CONVERT_SYSTEM_TIME_TO_MS(osvsp->lastScanTime);
        retv = 0;

        IOCTL_DPRINTF("%s: IEEE80211_IOCTL_SCANTIME - lastScanTime %d \n", IOCTL_LOG_PREFIX,
                        iiData->scantime.scan_time);
    }
    return retv;
}

static QDF_STATUS _vendor_ioctl_getparam_scan_resultCb(void *arg, wlan_scan_entry_t se)
{
    struct _scan_result_all *req = (struct _scan_result_all *)arg;
    athcfg_wcmd_scan_result_t *sr;
    u_int8_t *rate, *ie;
    wlan_chan_t chan;

    if (req->cnt >= IEEE80211_SCAN_ALLSIZE)
        return 0;

    sr = &(req->result[req->cnt]);

    ie = util_scan_entry_ssid(se)->ssid;
    sr->isr_ssid_len = util_scan_entry_ssid(se)->length;

    OS_MEMCPY(sr->isr_ssid, ie, sr->isr_ssid_len);

    chan = wlan_util_scan_entry_channel(se);
    sr->isr_freq = chan->ic_freq;
    sr->isr_ieee = chan->ic_ieee;

    sr->isr_rssi = util_scan_entry_rssi(se);
    sr->isr_capinfo = util_scan_entry_capinfo(se).value;
    sr->isr_erp = util_scan_entry_erpinfo(se);

    IEEE80211_ADDR_COPY(sr->isr_bssid, util_scan_entry_bssid(se));

    rate = util_scan_entry_rates(se);
    if (rate)
    {
        OS_MEMCPY(sr->isr_rates, &rate[2], rate[1]);
        sr->isr_nrates = rate[1];
    }

    rate = util_scan_entry_xrates(se);
    if (rate)
    {
        OS_MEMCPY(sr->isr_rates + sr->isr_nrates, &rate[2], rate[1]);
        sr->isr_nrates += rate[1];
    }

    ie = util_scan_entry_wpa(se);
    if (ie)
    {
        sr->isr_wpa_ie.len = (2 + ie[1]); 
        OS_MEMCPY(sr->isr_wpa_ie.data, ie, sr->isr_wpa_ie.len);
    }
 
    ie = util_scan_entry_wmeinfo(se);
    if (ie)
    {
        sr->isr_wme_ie.len = (2 + ie[1]);
        OS_MEMCPY(sr->isr_wme_ie.data, ie, sr->isr_wme_ie.len);
    }

    ie = util_scan_entry_athcaps(se);
    if (ie)
    {
        sr->isr_ath_ie.len = (2 + ie[1]);
        OS_MEMCPY(sr->isr_ath_ie.data, ie, sr->isr_ath_ie.len);
    }

    ie = util_scan_entry_rsn(se);
    if (ie)
    {
        sr->isr_rsn_ie.len = (2 + ie[1]);
        OS_MEMCPY(sr->isr_rsn_ie.data, ie, sr->isr_rsn_ie.len);
    }

    ie = util_scan_entry_wps(se);
    if (ie)
    {
        sr->isr_wps_ie.len = (2 + ie[1]);
        OS_MEMCPY(sr->isr_wps_ie.data, ie, sr->isr_wps_ie.len);
    }

    ie = util_scan_entry_htcap(se);
    if (ie)
    {
        sr->isr_htcap_ie.len = sizeof(struct ieee80211_ie_htcap_cmn);
        OS_MEMCPY(sr->isr_htcap_ie.data, ie, sr->isr_htcap_ie.len);
        OS_MEMCPY(sr->isr_htcap_mcsset, 
                  ((struct ieee80211_ie_htcap_cmn *)ie)->hc_mcsset,
                  ATHCFG_WCMD_MAX_HT_MCSSET);
    }

    ie = util_scan_entry_htinfo(se);
    if (ie)
    {
        sr->isr_htinfo_ie.len = sizeof(struct ieee80211_ie_htinfo_cmn);
        OS_MEMCPY(sr->isr_htinfo_ie.data, ie, sr->isr_htinfo_ie.len);
    }

    req->cnt += 1;

    return 0;
}

static int _vendor_ioctl_getparam_scan(struct net_device *dev, athcfg_wcmd_data_t *iiData)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct _vendor_generic_ioctl *osvsp = RAWDATA_TO_VENDOR_PRIV(osifp->os_vendor_specific);
    struct _scan_result_all *req;
    athcfg_wcmd_scan_t *scan;
    int retv = 0, left = 0;
    struct wlan_objmgr_psoc *psoc = NULL;

    if (osvsp->scan_is_set) /* not good idea. */
    {
        IOCTL_DPRINTF("%s: IEEE80211_IOCTL_SCAN - scanning...\n", IOCTL_LOG_PREFIX);

        return -EAGAIN;
    }

    req = (struct _scan_result_all *)OS_MALLOC(osifp->os_handle, sizeof(struct _scan_result_all), GFP_KERNEL);
    OS_MEMZERO(req, sizeof(struct _scan_result_all));

    scan = (athcfg_wcmd_scan_t *)OS_MALLOC(osifp->os_handle, sizeof(athcfg_wcmd_scan_t), GFP_KERNEL);
    OS_MEMZERO(scan, sizeof(athcfg_wcmd_scan_t));

    _WLAN_SCAN_TABLE_ITERATE(wlan_vap_get_pdev(vap), _vendor_ioctl_getparam_scan_resultCb, req);

    __xcopy_from_user(scan, iiData->scan, sizeof(athcfg_wcmd_scan_t));
    left = req->cnt - scan->offset;
    if (left > ATHCFG_WCMD_MAX_AP)
    {
        scan->more = 1;
        scan->cnt = ATHCFG_WCMD_MAX_AP;
    }
    else
    {
        scan->more = 0;
        scan->cnt = left;
        psoc = wlan_vap_get_psoc(vap);
        ucfg_scan_unregister_requester(psoc, osvsp->vendor_scan_requestor);
    }

    OS_MEMCPY(scan->result,
              (&req->result[scan->offset]),
              sizeof(athcfg_wcmd_scan_result_t) * scan->cnt)
    _copy_to_user(iiData->scan, scan, sizeof(athcfg_wcmd_scan_t));

#if 0
    IOCTL_DPRINTF("%s: IEEE80211_IOCTL_SCAN - offset %d cnt %d time %d \n", IOCTL_LOG_PREFIX,
                    scan->offset, scan->cnt, CONVERT_SYSTEM_TIME_TO_MS(osvsp->lastScanTime));
#endif

    OS_FREE(req);
    OS_FREE(scan);

    return retv;
}

static int _vendor_ioctl_setparam_scan(struct net_device *dev, athcfg_wcmd_data_t *iiData)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;
    struct _vendor_generic_ioctl *osvsp = RAWDATA_TO_VENDOR_PRIV(osifp->os_vendor_specific);
    struct scan_start_request *scan_params = NULL;
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    int retv = -EINVAL;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_vdev *vdev = NULL;

    if (dev->flags & IFF_UP)
    {
        if (osvsp->scan_is_set)
        {
            IOCTL_DPRINTF("%s: IEEE80211_IOCTL_SCAN - scan already going...\n", IOCTL_LOG_PREFIX);

            return retv;
        }
        else
            osvsp->scan_is_set = 1;

        vdev = osifp->ctrl_vdev;
        status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);
        if (QDF_IS_STATUS_ERROR(status)) {
            osvsp->scan_is_set = 0;
            scan_info("unable to get reference");
            return -EBUSY;
        }
        psoc = wlan_vap_get_psoc(vap);

        /* we are here means vendor scan is not in progress.
         * since this scan can go without interrupting any other scan,
         * as it has got its own scan id and requester id, go ahead and
         * start a new vendor scan. No need to cancel any other scan.
         *
         * if (wlan_vdev_scan_in_progress(vap->vdev_obj))
         *   _WLAN_SCAN_CANCEL(vap, osvsp);
         */

        scan_params = (struct scan_start_request*) qdf_mem_malloc(sizeof(*scan_params));
        if (!scan_params) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            osvsp->scan_is_set = 0;
            scan_err("unable to allocate scan request");
            return -ENOMEM;
        }

        status = wlan_update_scan_params(vap,scan_params,osifp->os_opmode,
                true,true,true,true,0,NULL,1);
        if (status) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            osvsp->scan_is_set = 0;
            qdf_mem_free(scan_params);
            scan_err("scan param init failed with status: %d", status);
            return -EINVAL;
        }

        scan_params->legacy_params.min_dwell_active =
            scan_params->scan_req.dwell_time_active = 100;
        scan_params->legacy_params.scan_type = SCAN_TYPE_FOREGROUND;
        scan_params->scan_req.scan_flags = 0;
        scan_params->scan_req.scan_f_passive = false;
        scan_params->scan_req.scan_f_2ghz = true;
        scan_params->scan_req.scan_f_5ghz = true;

        /* Calc. SCAN time */
        osvsp->vendor_scan_requestor =
            ucfg_scan_register_requester(psoc, (uint8_t*)"vendor_scantime",
                    _vendor_ioctl_getparam_scantime_evhandler, (void*)osifp);
        if (!osvsp->vendor_scan_requestor) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            osvsp->scan_is_set = 0;
            qdf_mem_free(scan_params);
            scan_err("unable to allocate requester");
            return -ENOMEM;
        }
        osvsp->lastScanStartTime = OS_GET_TICKS();

        /* Scan request */
        status = _WLAN_SCAN_START(vap, scan_params, osvsp);
        if (status) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            osvsp->scan_is_set = 0;
            scan_err("scan_start failed with status: %d", status);
            return -EBUSY;
        }

        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        retv = 0;
    }

    return retv;
}

static int _vendor_ioctl_getparam_rssi(struct net_device *dev, athcfg_wcmd_data_t *iiData)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    wlan_if_t vap = osifp->os_if;

    wlan_getrssi(vap, (wlan_rssi_info *)(&iiData->rssi.bc_avg_rssi), WLAN_RSSI_BEACON);
    wlan_getrssi(vap, (wlan_rssi_info *)(&iiData->rssi.data_avg_rssi), WLAN_RSSI_RX_DATA);

    return 0;
}

static int _vendor_ioctl_getparam_custdata(struct net_device *dev, athcfg_wcmd_data_t *iiData)
{
    osif_dev  *osifp = ath_netdev_priv(dev);
    /* To Parent */
    struct net_device *parentDev = osifp->os_comdev;
    struct ath_softc_net80211 *scn = ath_netdev_priv(parentDev);
    struct ath_softc          *sc  = ATH_DEV_TO_SC(scn->sc_dev);
    struct ath_hal            *ah  = sc->sc_ah;
    uint32_t    size;
    char        *eeprom;

    ath_hal_getdiagstate(ah, HAL_DIAG_EEPROM, NULL, 0, (void **)&eeprom, &size);

    if(eeprom && size>=(EEPROM_CUSTDATA_OFFSET+ACFG_CUSTDATA_LENGTH)){
        OS_MEMCPY(iiData->custdata.custdata, eeprom+EEPROM_CUSTDATA_OFFSET, ACFG_CUSTDATA_LENGTH);
    }
    return 0;
}

static int ieee80211_vendor_ioctl_setparam(struct net_device *dev, athcfg_wcmd_t *iiCmd)
{
    int retv = -EINVAL;

    switch (iiCmd->iic_cmd) 
    {
    case IEEE80211_IOCTL_TESTMODE:
        retv = _vendor_ioctl_setparam_testmode(dev, &iiCmd->iic_data);
        break;

    case IEEE80211_IOCTL_SCAN:
        retv = _vendor_ioctl_setparam_scan(dev, &iiCmd->iic_data);
        break;

    default:
        retv = -EOPNOTSUPP;
    }

    return retv;
}

static int ieee80211_vendor_ioctl_getparam(struct net_device *dev, athcfg_wcmd_t *iiCmd)
{
    int retv = 0;
 
    switch (iiCmd->iic_cmd) 
    {        
    case IEEE80211_IOCTL_TESTMODE:
        retv = _vendor_ioctl_getparam_testmode(dev, &iiCmd->iic_data);
        break;

    case IEEE80211_IOCTL_STAINFO:
        retv = _vendor_ioctl_getparam_stainfo(dev, &iiCmd->iic_data);
        break;

    case IEEE80211_IOCTL_SCANTIME:
        retv = _vendor_ioctl_getparam_scantime(dev, &iiCmd->iic_data);
        break;

    case IEEE80211_IOCTL_PHYMODE:
        retv = _vendor_ioctl_getparam_phymode(dev, &iiCmd->iic_data);
        break;

    case IEEE80211_IOCTL_SCAN:
        retv = _vendor_ioctl_getparam_scan(dev, &iiCmd->iic_data);
        break;
    case IEEE80211_IOCTL_RSSI:
        retv = _vendor_ioctl_getparam_rssi(dev, &iiCmd->iic_data);
        break;
    case IEEE80211_IOCTL_CUSTDATA:
        retv = _vendor_ioctl_getparam_custdata(dev, &iiCmd->iic_data);
        break;
        
    default:
        retv = -EOPNOTSUPP;
        break;
    }
    
    return retv; 
}

static int ieee80211_ioctl_vendor_generic(struct net_device *dev, ioctl_ifreq_req_t *iiReq)
{
    athcfg_wcmd_t *iiCmd, *in = IFREQ_TO_VENDOR_CMD(iiReq);
    int cmd, retv = -EOPNOTSUPP;

    iiCmd = qdf_mem_malloc(sizeof(athcfg_wcmd_t));
    __xcopy_from_user(iiCmd, in, sizeof(athcfg_wcmd_t));
    cmd = iiCmd->iic_cmd;
        
    IOCTL_DPRINTF("%s: %s - Vendor 0x%x CMD 0x%08x\n",IOCTL_LOG_PREFIX,  __func__,
                    iiCmd->iic_vendor, iiCmd->iic_cmd);   
 #if 0   
    if (iiCmd->iic_vendor != ATHCFG_WCMD_VENDORID)
        return retv;

    if (!(cmd & IOCTL_VIRDEV_MASK))
    {
        IOCTL_DPRINTF("%s: Virtual Device doesn't supoort this CMD.\n", IOCTL_LOG_PREFIX);
            
        return retv;
    }
 #endif   
    if (cmd & IOCTL_SET_MASK)
    {
        iiCmd->iic_cmd &= ~IOCTL_SET_MASK;
        
        retv = ieee80211_vendor_ioctl_setparam(dev, iiCmd);
    }
    else if (cmd & IOCTL_GET_MASK)    
    {
        iiCmd->iic_cmd &= ~IOCTL_GET_MASK;
        
        retv = ieee80211_vendor_ioctl_getparam(dev, iiCmd);
    }

    /* need? */
    if (retv == 0)
        _copy_to_user(in, iiCmd, sizeof(athcfg_wcmd_t));
    qdf_mem_free(iiCmd);

    return retv;
}
//////////////////////////////////////////////////////////////////////////////////////////////
int __osif_ioctl_vendor(struct net_device *dev, ioctl_ifreq_req_t *iiReq, int fromVirtual)
{
    if (fromVirtual)
        return ieee80211_ioctl_vendor_generic(dev, iiReq);
    else
        return ath_ioctl_vendor_generic(dev, iiReq);
}
