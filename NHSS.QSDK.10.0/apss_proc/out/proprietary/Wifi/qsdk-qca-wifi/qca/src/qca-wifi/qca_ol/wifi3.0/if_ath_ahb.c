/*
 * Copyright (c) 2013, 2016-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013, 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
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

#ifndef EXPORT_SYMTAB
#define	EXPORT_SYMTAB
#endif

#include <osdep.h>
#ifdef USE_PLATFORM_FRAMEWORK
#include <linux/platform_device.h>
#include <linux/ath9k_platform.h>
#endif

#if (ATH_PERF_PWR_OFFLOAD != 0) && defined(HIF_PCI)
#include "ol_if_athvar.h"
#include "ath_pci.h"
#endif

#include "target_type.h"
#include "if_athvar.h"
#include "ah_devid.h"
#include "if_ath_ahb.h"
#include <ath_pld.h>
#include <pld_common.h>

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <acfg_api_types.h>   /* for ACFG_WDT_REINIT_DONE */
#include <acfg_drv_event.h>
#include <hif.h>
#include <linux/notifier.h>
#include <asm/kdebug.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/if_bridge.h>
#include <linux/netfilter_bridge.h>

extern struct platform_driver ol_ath_ahb_driver;
extern bool qca6290_support;
void ol_if_register_wifi3_0(void);
void wmi_tlv_init(void);
void ce_service_srng_init(void);

/*
 * Module glue.
 */
#include "version.h"

#if !defined(ATH_PCI) || !defined(USE_PLATFORM_FRAMEWORK)
static char *version = ATH_PCI_VERSION " (Atheros/multi-bss)";
static char *dev_info = "ath_ol_ahb";
#endif
#include <linux/ethtool.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

extern struct nss_wifi_soc_ops nss_wifili_soc_ops;
extern void osif_nss_register_module(OL_WIFI_DEV_TYPE target_type,
			struct nss_wifi_soc_ops *soc_ops);

#if DBDC_REPEATER_SUPPORT
int qca_net_dev_event_handler(struct notifier_block *self,
				unsigned long val, void *data)
{
	struct net_device *dev = netdev_notifier_info_to_dev(data);
	osif_dev *osifp;
	wlan_if_t vap;
	qdf_nbuf_t nbuf;
	struct l2_update_frame *l2uf;
	struct ether_header *eh;

	if (val == NETDEV_CHANGE && (strncmp(dev->name, "ath", 3) == 0)) {
		osifp = ath_netdev_priv(dev);
		vap = osifp->os_if;

		if (!vap)
		{
		       QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,FL("cannot transmit l2uf on vap: %s\n"), dev->name);
		       return 0;
		}
		/* add 2 more bytes to meet minimum packet size allowed with LLC headers */
		nbuf = qdf_nbuf_alloc(NULL, sizeof(*l2uf) + 2, 0, 0, 0);
		if (!nbuf)
		{
		       QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,FL("ieee80211_hardstart_l2uf: no buf available\n"));
		       return 0;
		}

		qdf_nbuf_put_tail(nbuf, sizeof(*l2uf) + 2);
		l2uf = (struct l2_update_frame *)(qdf_nbuf_data(nbuf));
		eh = &l2uf->eh;
		/* dst: Broadcast address */
		IEEE80211_ADDR_COPY(eh->ether_dhost, dev->broadcast);
		/* src: associated STA */
		IEEE80211_ADDR_COPY(eh->ether_shost, vap->iv_myaddr);
		eh->ether_type = htons(qdf_nbuf_len(nbuf) - sizeof(*eh));

		l2uf->dsap = 0;
		l2uf->ssap = 0;
		l2uf->control = IEEE80211_L2UPDATE_CONTROL;
		l2uf->xid[0] = IEEE80211_L2UPDATE_XID_0;
		l2uf->xid[1] = IEEE80211_L2UPDATE_XID_1;
		l2uf->xid[2] = IEEE80211_L2UPDATE_XID_2;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
		nbuf->mac.raw = qdf_nbuf_data(nbuf);
#else
		skb_reset_mac_header(nbuf);
#endif
		if (vap->iv_evtable) {
			vap->iv_evtable->wlan_vap_xmit_queue(vap->iv_ifp, nbuf);
		}
	}

	return 0;
}

static struct notifier_block qca_net_dev_notifier =
{
	.notifier_call = qca_net_dev_event_handler,
};
#endif

int init_ath_ahb_3_0(void)
{
	int ret = 0;
#ifdef WIFI_TARGET_TYPE_3_0
	qdf_debug("WIFI3.0 Registration");
	/* register wifi3.0 architecture ops */
	ol_if_register_wifi3_0();
	wmi_tlv_init();
	ce_service_srng_init();
#if QCA_NSS_WIFILI_OFFLOAD_SUPPORT
	osif_nss_register_module(OL_WIFI_3_0, &nss_wifili_soc_ops);
#endif
#endif

	if(qca6290_support) {
		pld_power_on(NULL, PLD_BUS_TYPE_PCIE, QCA6290_DEVICE_ID);
		pld_rescan_bus(PLD_BUS_TYPE_PCIE);
	}

	ret = pld_module_init();
	if(ret) {
		qdf_print("pld_module_init failed %d",ret);
		return ret;
	}

#if DBDC_REPEATER_SUPPORT
	register_netdevice_notifier(&qca_net_dev_notifier);
#endif

	return 0;
}

void exit_ath_ahb_3_0(void)
{
#if DBDC_REPEATER_SUPPORT
	unregister_netdevice_notifier(&qca_net_dev_notifier);
#endif
	pld_module_exit();

	if(qca6290_support) {
		pld_remove_bus(PLD_BUS_TYPE_PCIE);
		pld_power_off(NULL, PLD_BUS_TYPE_PCIE, QCA6290_DEVICE_ID);
	}
}

#if !defined(ATH_PCI)
module_init(init_ath_ahb_3_0);
module_exit(exit_ath_ahb_3_0);
#endif /* ATH_PCI */
