/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2006-2007 Atheros Communications, Inc.
 * Copyright (c) 2010, Atheros Communications Inc.
 * All rights reserved.
 *
 */
/*
 * ath_tx99: continuous transmit diagnostic utility.
 *
 * To use, build the module.  Load the driver as usual followed by
 * the ath_tx99 module.  On module load specify the base device to
 * use for testing; e.g.
 *	insmod ath_tx99.ko device=wifi0
 *
 * To start continuous transmit bring the device up; e.g.
 *	ifconfig wifi0 up
 * To stop continuous transmit mark the device down:
 *	ifconfig wifi0 down
 * or unload the ath_tx99 module.  Note that so long as the
 * module is loaded you can only do continuous transmit.
 *
 * By default the current channel is used.  To specify a channel
 * for transmits use sysctl; e.g.
 *	sysctl -w dev.wifi0.tx99freq=2437
 * The frequency is used to find a channel based on the current
 * band/wireless mode.  This can be overridden with:
 *	sysctl -w dev.wifi0.tx99txmode=8
 * IEEE80211_MODE_11NG; e.g. 8 is the only mode currently supported.
 *
 * The default CWM mode is HT20(0), to set in HT40 mode(2), use:
 *      sysctl -w dev.wifi0.tx99htmode=2
 * Extension offset can be set in 0(none), 1(plus) and 2(minus) by:
 *      sysctl -w dev.wifi0.tx99htext=1
 *
 * Finally the transmit rate can be specified in Kbps; e.g.
 *	sysctl -w dev.wifi0.tx99rate=108000
 * and the transmit power in dBm with
 *	sysctl -w dev.wifi0.tx99power=30
 *
 */
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifdef ATH_TX99_DIAG
#include "ath_internal.h"
#include "if_athvar.h"
#include "ah.h"
#include "ah_desc.h"
#include "ah_devid.h"

#ifdef ATH_PCI      /* PCI BUS */
#include "if_ath_pci.h"
#endif          /* PCI BUS */
#ifdef ATH_AHB      /* AHB BUS */
#include "if_ath_ahb.h"
#endif          /* AHB BUS */
#include "tx99_wcmd.h"
#include "ath_tx99.h"

#include <qdf_module.h>
/* #include <qdf_stdtypes.h> */
#include <qdf_types.h>
/* #include <qdf_dma.h> */
#include <qdf_time.h>
#include <qdf_lock.h>
/* #include <qdf_io.h> */
#include <qdf_mem.h>
#include <qdf_util.h>
#include <qdf_defer.h>
#include <qdf_atomic.h>
#include <qdf_nbuf.h>
#include <qdf_net_types.h>
/* #include <osif_net.h> */
#include "if_athvar.h"
#include "ah_desc.h"
#include "ah_devid.h"
#ifdef ATH_PCI		/* PCI BUS */
#include "if_ath_pci.h"
#endif			/* PCI BUS */
#ifdef ATH_AHB		/* AHB BUS */
#include "if_ath_ahb.h"
#endif			/* AHB BUS */
#include "tx99_wcmd.h"
#include "ath_tx99.h"
#include "ath_internal.h"
#include "ieee80211_channel.h"
#include "ah.h"
#define TX99_IS_CHAN_2GHZ(_c)       (((_c)->channel_flags & CHANNEL_2GHZ) != 0)
#define TX99_IS_CHAN_5GHZ(_c)       (((_c)->channel_flags & CHANNEL_5GHZ) != 0)
#define TX99_IS_CHAN_HT_CAPABLE(_c) ((((_c)->channel_flags & CHANNEL_HT20) != 0)     ||  \
                                     (((_c)->channel_flags & CHANNEL_HT40PLUS) != 0) ||  \
                                     (((_c)->channel_flags & CHANNEL_HT40MINUS) != 0))
struct ath_tx99_tgt {
    u_int32_t	txrate;		/* tx rate */
    u_int32_t	txpower;	/* tx power */
    u_int32_t	txchain;	/* tx chainmask */
    u_int32_t	htmode;		/* channel width mode, e.g. HT20, HT40 */
    u_int32_t	type;		/* single carrier or frame type */
    u_int32_t	chtype;		/* channel type */
    u_int32_t	txantenna;	/* tx antenna */
};
static void tx99_stop(struct ath_softc *sc, int flag);

/*
 * Drain the transmit queues and reclaim resources.
 */
static void
ath_drain_txq(struct ath_softc *sc)
{
    /* stop beacon queue. The beacon will be freed when we go to INIT state */
    if (!sc->sc_invalid) {
        if ( !(sc->sc_fastabortenabled && (sc->sc_reset_type == ATH_RESET_NOLOSS)) ){
            (void) ath_hal_stoptxdma(sc->sc_ah, sc->sc_bhalq, 0);
        }
        if (sc->sc_fastabortenabled) {
            /* fast tx abort */
            ath_hal_aborttxdma(sc->sc_ah);
        }
    }
    /* to drain tx dataq */
    sc->sc_ath_ops.tx_flush(sc);
}
static qdf_nbuf_t
ath_alloc_skb_tx99(qdf_handle_t os_hdl, u_int size, u_int align)
{
    qdf_nbuf_t skb;
    skb = qdf_nbuf_alloc(os_hdl, size, 0, align, FALSE);
    return skb;
}
static int
tx99_set_cwm_param(struct ath_tx99 *tx99)
{
    if ((tx99->htmode == IEEE80211_CWM_MODE20) || (tx99->htmode == IEEE80211_CWM_MODE40)) {
        if (tx99->htmode == IEEE80211_CWM_MODE20) {
            qdf_print("tx99 in HT20 mode");
            if (tx99->txfreq < 4900)
            {
                tx99->txmode = IEEE80211_MODE_11NG_HT20;
                tx99->phymode = IEEE80211_MODE_11NG_HT20;
            }
            else {
                tx99->txmode = IEEE80211_MODE_11NA_HT20;
                tx99->phymode = IEEE80211_MODE_11NA_HT20;
            }
        } else {
            qdf_print("tx99 in HT40 mode");
        }
    }

    if ( tx99->htext < 3 ) { //2: extension is minus. 1: extension is plus 0: 11B
        if (tx99->htmode == IEEE80211_CWM_MODE40) {
            switch (tx99->htext) {
            case 1:
                qdf_print("tx99: set extension mode to plus");
                if (tx99->txfreq < 4900)
                {
                    tx99->txmode = IEEE80211_MODE_11NG_HT40PLUS;
                    tx99->phymode = IEEE80211_MODE_11NG_HT40PLUS;
                }
                else
                {
                    tx99->txmode = IEEE80211_MODE_11NA_HT40PLUS;
                    tx99->phymode = IEEE80211_MODE_11NA_HT40PLUS;
                }
                break;
            case 2:
                qdf_print("tx99: set extension mode to minus");
                if (tx99->txfreq < 4900)
                {
                    tx99->txmode = IEEE80211_MODE_11NG_HT40MINUS;
                    tx99->phymode = IEEE80211_MODE_11NG_HT40MINUS;
                }
                else {
                    tx99->txmode = IEEE80211_MODE_11NA_HT40MINUS;
                    tx99->phymode = IEEE80211_MODE_11NA_HT40MINUS;
                }
                break;
            }
        }
        else {   //in IEEE80211_CWM_MODE20 mode
            if(tx99->htext != 0) {
                qdf_print("tx99: Can not set extension offset in HT20, always 0.");
                return -EINVAL;
            }
        }
    }
    else {
        qdf_print("extension offset must be 0(none), 1(plus), 2(minus).");
        return -EINVAL;
    }
    return EOK;
}
static int
tx99_channel_setup(struct ath_softc *sc)
{
    struct ath_tx99 *tx99 = sc->sc_tx99;
    struct ieee80211com *ic = (struct ieee80211com *)sc->sc_ieee;
    struct ath_hal *ah;
    u_int8_t ieee;
    struct ieee80211_ath_channel *c;
    if(tx99_set_cwm_param(tx99) != EOK)
    {
        qdf_print("%s: set cwm parm fail", __FUNCTION__);
        return -EINVAL;
    }

    /*
     * Locate the channel to use if user-specified; otherwise
     * user the current channel.
     */
    if (tx99->txfreq != 0) {
        ah = sc->sc_ah;
        ieee = ath_hal_mhz2ieee(ah, tx99->txfreq, 0);
        qdf_print("%s: ieee = %d", __FUNCTION__, ieee);
        qdf_print("%s: tx99->txfreq=%d,tx99->txmode=%d", __FUNCTION__, tx99->txfreq, tx99->txmode);
       c = sc->sc_ieee_ops->ath_net80211_find_channel(sc, ieee, 0, tx99->txmode);

        if (c == NULL) {
            qdf_print("%s: channel %u mode %u not available",
            __FUNCTION__, tx99->txfreq, tx99->txmode);
            return -EINVAL;
        }
        qdf_print("%s: switching to channel %u (flags 0x%llx mode %u)",
            __FUNCTION__, c->ic_freq, c->ic_flags, tx99->txmode);
        ic->ic_curchan = c;
	    if(ic->ic_set_channel(ic))
        {
            qdf_print("%s: reset channel fail, check frequency settings", __FUNCTION__);
            return -EINVAL;
        }
    }
    return EOK;
}
/*
 * Start continuous transmit operation.
 */
static int
tx99_start(struct ath_softc *sc)
{
    static struct ath_tx99_tgt tx99_tgt;
    struct ath_tx99 *tx99 = sc->sc_tx99;
    struct ath_hal *ah = sc->sc_ah;
    int is2GHz = 0;
    qdf_nbuf_t 	skb;
    HAL_CHANNEL *c = NULL;
    if(tx99->recv)
    {
        ath_hal_phydisable(ah);
        //qdf_print("%s: device %s tx99 continuous receive mode", __FUNCTION__, qdf_net_ifname(sc->sc_ic->ic_dev));
        return 0;
    }
    /* check tx99 running state */
    if(tx99->tx99_state){		/* already active */
        qdf_print("%s: already running", __FUNCTION__);
        return 0;
    }
    /* set tx99 state active */
    tx99->tx99_state = 1;
    /* allocate diag packet buffer */
    tx99->skb = ath_alloc_skb_tx99(sc->sc_osdev, 2000, 32);
    if (tx99->skb == NULL) {
        qdf_print("%s: unable to allocate skb", __FUNCTION__);
        tx99->tx99_state = 0;
        return -ENOMEM;
    }
    skb = tx99->skb;
    /* drain all tx queue */
    ath_drain_txq(sc);
    /*
     * Setup channel using configured frequency+flags.
     */
    if (tx99_channel_setup(sc) != EOK) {
        qdf_print("%s: unable to setup operation", __FUNCTION__);
        tx99->tx99_state = 0;
        qdf_nbuf_free(skb);
        return -EIO;
    }
    /*disable desc tpc */
    ath_hal_settpc(ah,0);
    /*
     * Setup tx power limit
     */
    c = &sc->sc_curchan;
    is2GHz = TX99_IS_CHAN_2GHZ(c);
    ath_hal_settxpowlimit(ah,tx99->txpower,0,is2GHz);
    /* set tx99 enable */
    tx99_tgt.txrate = qdf_htonl(tx99->txrate);
    tx99_tgt.txpower = qdf_htonl(tx99->txpower);
    tx99_tgt.txchain = qdf_htonl(tx99->chanmask);
    tx99_tgt.htmode = qdf_htonl(tx99->htmode);
    tx99_tgt.type = qdf_htonl(tx99->type);
    tx99_tgt.chtype = qdf_htonl(TX99_IS_CHAN_5GHZ(c));
    tx99_tgt.txantenna = qdf_htonl(0);
    if( tx99->txpower < 60 ) /* only update channel pwr if not default MAX power */
        ath_hal_tx99_channel_pwr_update(ah, c, tx99->txpower);
    qdf_nbuf_put_tail(skb, 1500);
    if (ath_tx99_tgt_start(sc, tx99_tgt.chtype) != EOK) {
        qdf_print("%s: tx99 fail ", __FUNCTION__);
        tx99_stop(sc, 0);
    }
    /* wait a while to make sure target setting ready */
    qdf_mdelay(50);
    qdf_print("%s: continuous transmit started", __FUNCTION__);
    return 0;
}
/*
 * Terminate continuous transmit operation.
 */
static void
tx99_stop(struct ath_softc *sc, int flag)
{
    struct ath_tx99 *tx99 = sc->sc_tx99;
    struct ieee80211com *ic = (struct ieee80211com *)sc->sc_ieee;
    qdf_nbuf_t  skb = tx99->skb;
    if (tx99->tx99_state == 0)
        return;
    ath_tx99_tgt_stop(sc);
    if (flag == 0) {
        ic->ic_reset_start(ic,0);
        ic->ic_reset(ic);
        ic->ic_reset_end(ic,0);
    } else if (flag == 1) {
        ic->ic_reset_start(ic,0);
        ic->ic_reset(ic);
    } else if (flag == 2) {
        ic->ic_reset_end(ic,0);
    }
    qdf_nbuf_free(skb);
    tx99->tx99_state = 0;
    qdf_print("%s: continuous transmit stopped", __FUNCTION__);
}

u_int8_t
tx99_ioctl(ath_dev_t device, struct ath_softc *sc, int cmd, void *addr)
{
    struct ath_tx99 *tx99 = sc->sc_tx99;
    struct net_device *dev = (struct net_device *)device;
    static tx99_wcmd_t i_req;

    if(copy_from_user(&i_req, addr, sizeof(i_req))) {
            return -EFAULT;
    }
    switch(i_req.type){
        case TX99_WCMD_ENABLE:
            qdf_print("tx99_ioctl command : enable");
            if ((dev->flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP)) {
#ifdef __linux__
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
                dev->netdev_ops->ndo_open(dev);
#else
                dev->open(dev);
#endif
#else
                qdf_print("Interface is not active");
                return -EINVAL;
#endif
            }
            if (tx99->testmode != TX99_TESTMODE_SINGLE_CARRIER) {
                /**/
                tx99_start(sc);
                qdf_mdelay(1000);
                tx99_stop(sc, 0);
                qdf_mdelay(50);
            }
            tx99_start(sc);
            break;
        case TX99_WCMD_DISABLE:
            qdf_print("tx99_ioctl command : disable");
            tx99_stop(sc, 0);
            break;
        case TX99_WCMD_SET_FREQ:
            qdf_print("tx99_ioctl command : freq %d %d %d",i_req.data.freq,i_req.data.htmode,i_req.data.htext);
            tx99->txfreq = i_req.data.freq;
            tx99->htmode = i_req.data.htmode;
            tx99->htext = i_req.data.htext;
            break;
        case TX99_WCMD_SET_RATE:
            qdf_print("tx99_ioctl command : rate %d",i_req.data.rate);
            tx99->txrate = i_req.data.rate;
            break;
        case TX99_WCMD_SET_POWER:
            qdf_print("tx99_ioctl command : pwr %d",i_req.data.power);
            tx99->txpower = (i_req.data.power*2);
            break;
        case TX99_WCMD_SET_TXMODE:
            qdf_print("tx99_ioctl command : txmode %d",i_req.data.txmode);
            tx99->txmode = i_req.data.txmode;
            break;
        case TX99_WCMD_SET_CHANMASK:
            qdf_print("tx99_ioctl command : chanmask %d",i_req.data.chanmask);
            tx99->chanmask = i_req.data.chanmask;
            break;
        case TX99_WCMD_SET_TYPE:
            qdf_print("tx99_ioctl command : type %d",i_req.data.type);
            tx99->type = i_req.data.type;
            break;
        case TX99_WCMD_SET_TESTMODE:
            qdf_print("tx99_ioctl command : testmode %d",i_req.data.testmode);
            tx99->testmode = i_req.data.testmode;
            if (tx99->testmode == TX99_TESTMODE_RX_ONLY)
                tx99->recv = 1;
            else
                tx99->recv = 0;
            break;
        case TX99_WCMD_GET:
            if(tx99 == NULL)
                return -EINVAL;
            else
            {
                i_req.data.rate = tx99->txrate;
                i_req.data.freq = tx99->txfreq;
                i_req.data.htmode = tx99->htmode;
                i_req.data.htext = tx99->htext;
                i_req.data.power = (tx99->txpower/2);
                i_req.data.txmode = tx99->txmode;
            }
            break;
        default:
            return -EINVAL;
            break;
    }
    return EOK;
}

int
tx99_attach(struct ath_softc *sc)
{
    struct ath_tx99 *tx99 = sc->sc_tx99;
    if (tx99 != NULL) {
		qdf_print("%s: sc_tx99 was not NULL", __FUNCTION__);
		return EINVAL;
	}
    tx99 = qdf_mem_malloc(sizeof(struct ath_tx99));
    if (tx99 == NULL) {
		qdf_print("%s: no memory for tx99 attach", __FUNCTION__);
		return ENOMEM;
	}
    qdf_mem_set(tx99, sizeof(struct ath_tx99), 0);
    sc->sc_tx99 = tx99;

    tx99->stop = tx99_stop;
    tx99->start = tx99_start;
    tx99->tx99_state = 0;
    tx99->txpower = 60;
    tx99->txrate = 300000;
    tx99->txfreq = 6;/* ieee channel number */
    tx99->txmode = IEEE80211_MODE_11NG_HT40PLUS;
    tx99->phymode = IEEE80211_MODE_AUTO;
    tx99->chanmask = 1;
    tx99->recv = 0;
    tx99->testmode = TX99_TESTMODE_TX_PN;

    return EOK;
}

void
tx99_detach(struct ath_softc *sc)
{
    if(sc) {
    tx99_stop(sc, 0);

        if (sc->sc_tx99) {
            qdf_mem_free(sc->sc_tx99);
        }
        sc->sc_tx99 = NULL;
    }
}

/*
 * Module glue.
 */
static	char *dev_info = "ath_tx99";

static int __init
init_ath_tx99(void)
{
    if(dev_info) {
        qdf_print("%s: Version 2.0\n"
                "Copyright (c) 2010 Atheros Communications, Inc, "
                "All Rights Reserved", dev_info);
    }
	return EOK;
}

static void __exit
exit_ath_tx99(void)
{
	qdf_print(KERN_INFO"%s: driver unloaded", dev_info);
}

qdf_virt_module_init(init_ath_tx99);
qdf_virt_module_exit(exit_ath_tx99);
qdf_virt_module_name(ath_tx99);
EXPORT_SYMBOL(tx99_attach);
EXPORT_SYMBOL(tx99_detach);
EXPORT_SYMBOL(tx99_ioctl);
#endif
