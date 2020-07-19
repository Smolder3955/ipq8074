/*
 * Copyright (c) 2013-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013-2016 Qualcomm Atheros, Inc.
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
#define EXPORT_SYMTAB
#endif

#include <linux/version.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <if_athvar.h>
#include "if_ath_pci.h"
#include "ol_if_athvar.h"
extern bool driver_registered;
extern struct semaphore reset_in_progress;

void wmi_non_tlv_init(void);
void ce_service_legacy_init(void);
int ath_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id);
void ath_pci_remove(struct pci_dev *pdev);
int
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)
ath_pci_suspend(struct pci_dev *pdev, pm_message_t state);
#else
ath_pci_suspend(struct pci_dev *pdev, u32 state);
#endif
int ath_pci_resume(struct pci_dev *pdev);
QDF_STATUS create_target_if_ctx(void);
void ol_if_register_wifi2_0(void);
#ifdef ATH_AHB
extern int qca_ol_exit_in_progress;
#endif
#include "if_athvar.h"
#include "if_bus.h"
#include "osif_private.h"
#include <acfg_api_types.h>   /* for ACFG_WDT_REINIT_DONE */
#include <acfg_drv_event.h>
#include <wlan_cmn.h>

#ifdef WIFI_TARGET_TYPE_2_0
/*
 * Use a static table of PCI id's for now.  While this is the
 * "new way" to do things, we may want to switch back to having
 * the HAL check them by defining a probe method.
 */
DEFINE_PCI_DEVICE_TABLE(ath_pci_id_table_2_0) = {
    { 0x168c, 0x003c, PCI_ANY_ID, PCI_ANY_ID }, /* Peregrine PCIE  */
    { 0x168c, 0x0050, PCI_ANY_ID, PCI_ANY_ID }, /* Swift PCIE  */
    { 0x168c, 0x0040, PCI_ANY_ID, PCI_ANY_ID }, /* beeliner PCIE  */
    { 0x168c, 0x0046, PCI_ANY_ID, PCI_ANY_ID }, /* Cascade PCIE  */
    { 0x168c, 0x0056, PCI_ANY_ID, PCI_ANY_ID }, /* Besra PCIE  */
#if 0
    { 0xbeaf, 0xabc0, PCI_ANY_ID, PCI_ANY_ID }, /* Rumi PCIE  */
    { 0xbeaf, 0xabc1, PCI_ANY_ID, PCI_ANY_ID }, /* Rumi PCIE  */
    { 0xbeaf, 0xabc2, PCI_ANY_ID, PCI_ANY_ID }, /* Rumi PCIE  */
    { 0xbeaf, 0xabc3, PCI_ANY_ID, PCI_ANY_ID }, /* Rumi PCIE  */
    { 0x168c, 0xabcd, PCI_ANY_ID, PCI_ANY_ID }, /* Napier  */
#endif
    { 0 }
};

MODULE_DEVICE_TABLE(pci, ath_pci_id_table_2_0);

static struct pci_driver ol_ath_pci_driver = {
    .name       = "ath_ol_pci_2_0",
    .id_table   = ath_pci_id_table_2_0,
    .probe      = ath_pci_probe,
    .remove     = ath_pci_remove,
#ifdef ATH_BUS_PM
    .suspend    = ath_pci_suspend,
    .resume     = ath_pci_resume,
#endif /* ATH_BUS_PM */
    /* Linux 2.4.6 has save_state and enable_wake that are not used here */
};
#endif

/*
 * Module glue.
 */
#include "version.h"
#ifdef WIFI_TARGET_TYPE_2_0
static char *version = ATH_PCI_VERSION " (Atheros/multi-bss)";
static char *dev_info = "ath_ol_pci_2_0";
#endif
#include <linux/ethtool.h>

#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif

extern struct nss_wifi_soc_ops nss_wifibe_soc_ops;
extern void osif_nss_register_module(OL_WIFI_DEV_TYPE target_type,
					struct nss_wifi_soc_ops *soc_ops);

static int __init
init_ath_pci_2_0(void)
{

#ifdef WIFI_TARGET_TYPE_2_0
#ifdef ATH_AHB
    int pciret = 0, ahbret = 0;
    int init_ath_ahb_2_0(void);
#endif

    /* register wifi2.0 architecture ops */
    ol_if_register_wifi2_0();
    wmi_non_tlv_init();
    ce_service_legacy_init();
#if QCA_NSS_WIFI_2_0_OFFLOAD_SUPPORT
    osif_nss_register_module(OL_WIFI_2_0, &nss_wifibe_soc_ops);
#endif
    qdf_debug("WIFI2.0 Registration");

#ifdef ATH_AHB
    ahbret = init_ath_ahb_2_0();
    if(ahbret < 0 ) {
        qdf_print("ath_ahb: Error while registering ath wlan ahb driver");
    }
#endif

    qdf_print(KERN_INFO "%s: %s", dev_info, version);

    if (pci_register_driver(&ol_ath_pci_driver) < 0) {
        qdf_print("ath_pci: No devices found, driver not installed.");
        pci_unregister_driver(&ol_ath_pci_driver);
#ifdef ATH_AHB
        pciret = -ENODEV;
#else
        return (-ENODEV);
#endif
    }


#ifdef ATH_AHB
    /*
     * Return failure only when there is no wlan device
     * on both pci and ahb buses.
     */
      if (ahbret && pciret) {
              /* which error takes priority ?? */
              return ahbret;
      }
#endif

    driver_registered = true;
    sema_init(&reset_in_progress, 1);
#endif
    return 0;
}
module_init(init_ath_pci_2_0);


static void __exit
exit_ath_pci_2_0(void)
{

#ifdef WIFI_TARGET_TYPE_2_0
#ifdef ATH_AHB
    void exit_ath_ahb_2_0(void);
    qca_ol_exit_in_progress = 1;
#endif

    if (down_interruptible(&reset_in_progress))
        return;

    pci_unregister_driver(&ol_ath_pci_driver);
    driver_registered = false;
    up(&reset_in_progress);

#ifdef ATH_AHB
        exit_ath_ahb_2_0();
#else
    qdf_print(KERN_INFO "%s: driver unloaded", dev_info);
#endif /* ATH_AHB */
#endif
}
module_exit(exit_ath_pci_2_0);
