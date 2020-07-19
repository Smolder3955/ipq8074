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

int ol_ath_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id);
void ol_ath_pci_remove(struct pci_dev *pdev);
int ol_ath_pci_suspend(struct pci_dev *pdev, pm_message_t state);
int ol_ath_pci_resume(struct pci_dev *pdev);
#if UMAC_SUPPORT_WEXT
void ol_ath_iw_detach(struct net_device *dev);
void ol_ath_iw_attach(struct net_device *dev);
#endif
QDF_STATUS create_target_if_ctx(void);
int driver_initialized = 0;
#include "if_athvar.h"
#include "if_bus.h"
#include "osif_private.h"
#include <acfg_api_types.h>   /* for ACFG_WDT_REINIT_DONE */
#include <acfg_drv_event.h>
#include <wlan_cmn.h>

#ifndef ATH_BUS_PM
#ifdef CONFIG_PM
#define ATH_BUS_PM
#endif /* CONFIG_PM */
#endif /* ATH_BUS_PM */

#ifdef ATH_BUS_PM
#define ATH_PCI_PM_CONTROL 0x44
#endif

#ifdef AH_CAL_IN_FLASH_PCI
u_int32_t CalAddr[AH_CAL_RADIOS_PCI] = {AH_CAL_LOCATIONS_PCI};
int pci_dev_cnt=0;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0))
/*
 * PCI initialization uses Linux 2.4.x version and
 * older kernels do not support this
 */
#error Atheros PCI version requires at least Linux kernel version 2.4.0
#endif /* kernel < 2.4.0 */

#if QCA_PARTNER_DIRECTLINK_TX
extern void wlan_pltfrm_directlink_set_pcie_base(uint32_t mem);
#endif /* QCA_PARTNER_DIRECTLINK_TX */

int
ath_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    u_int8_t max_payload;
#ifdef AH_CAL_IN_FLASH_PCI
    pci_dev_cnt++;
#endif
    pci_read_config_byte(pdev, 0x74, &max_payload);
#if (ATH_PERF_PWR_OFFLOAD != 0) && defined(HIF_PCI)
#define AR9888_DEVICE_ID	(0x003c)
#define AR9887_DEVICE_ID    (0x0050)
#define AR900B_DEVICE_ID    (0x0040)
#define QCA6290_DEVICE_ID   (0x1100)
#define QCA9984_DEVICE_ID   (0x0046)
#define QCA9888_DEVICE_ID   (0x0056)
    /* emulation device id, peregrine id or beeliner id*/
    if (((id->device == 0xabcd) && (max_payload != 0)) ||
         (id->device == AR9888_DEVICE_ID) ||
         (id->device == AR9887_DEVICE_ID) ||
         (id->device == QCA6290_DEVICE_ID)||
         (id->device == AR900B_DEVICE_ID) ||
         (id->device == QCA9984_DEVICE_ID) ||
         (id->device == QCA9888_DEVICE_ID) ||
         (id->device == 0xabc0) ||
         (id->device == 0xabc1) ||
         (id->device == 0xabc2) ||
         (id->device == 0xabc3) ||
         (id->device == 0xaa10) ||
         (id->device == 0xaa11)) {
#if QCA_PARTNER_DIRECTLINK_TX
        wlan_pltfrm_directlink_set_pcie_base((uint32_t) pci_resource_start(pdev, 0));
#endif /* QCA_PARTNER_DIRECTLINK_TX */

        return ol_ath_pci_probe(pdev, id);
    }
#endif /* ATH_PERF_PWR_OFFLOAD && HIF_PCI */

    return (-ENODEV);
}
EXPORT_SYMBOL(ath_pci_probe);

void
ath_pci_remove(struct pci_dev *pdev)
{
    u_int8_t max_payload;

    pci_read_config_byte(pdev, 0x74, &max_payload);
#if (ATH_PERF_PWR_OFFLOAD != 0) && defined(HIF_PCI)
    if (((pdev->device == 0xabcd) && (max_payload != 0)) ||
         (pdev->device == AR9888_DEVICE_ID) ||
         (pdev->device == AR900B_DEVICE_ID) ||
         (pdev->device == QCA6290_DEVICE_ID)||
         (pdev->device == AR9887_DEVICE_ID) ||
         (pdev->device == QCA9984_DEVICE_ID)||
         (pdev->device == QCA9888_DEVICE_ID)||
         (pdev->device == 0xabc0)||
         (pdev->device == 0xabc1)||
         (pdev->device == 0xabc2)||
         (pdev->device == 0xabc3)) {
        ol_ath_pci_remove(pdev);
        return;
    }
#endif /* ATH_PERF_PWR_OFFLOAD && HIF_PCI */
}
EXPORT_SYMBOL(ath_pci_remove);

#ifdef ATH_BUS_PM
int
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,10)
ath_pci_suspend(struct pci_dev *pdev, pm_message_t state)
#else
ath_pci_suspend(struct pci_dev *pdev, u32 state)
#endif
{
    u_int8_t max_payload;

    pci_read_config_byte(pdev, 0x74, &max_payload);
#if (ATH_PERF_PWR_OFFLOAD != 0) && defined(HIF_PCI)
    if (((pdev->device == 0xabcd) && (max_payload != 0)) || (pdev->device == AR9888_DEVICE_ID) || (pdev->device == AR900B_DEVICE_ID) || (pdev->device == AR9887_DEVICE_ID) || (pdev->device == QCA9984_DEVICE_ID) || (pdev->device == QCA9888_DEVICE_ID)) {
        if(ol_ath_pci_suspend(pdev, state)) {
            return (-1);
        }
        return 0;
    }
#endif /* ATH_PERF_PWR_OFFLOAD && HIF_PCI */

    return 0;
}
EXPORT_SYMBOL(ath_pci_suspend);

int
ath_pci_resume(struct pci_dev *pdev)
{
    u_int8_t max_payload;

    pci_read_config_byte(pdev, 0x74, &max_payload);
#if (ATH_PERF_PWR_OFFLOAD != 0) && defined(HIF_PCI)
    if (((pdev->device == 0xabcd) && (max_payload != 0)) || pdev->device == AR9888_DEVICE_ID || (pdev->device == AR900B_DEVICE_ID) || (pdev->device == AR9887_DEVICE_ID) || (pdev->device == QCA9984_DEVICE_ID) || (pdev->device == QCA9888_DEVICE_ID)) {
        if (ol_ath_pci_resume(pdev)) {
            return (-1);
        }
        return 0;
    }
#endif /* ATH_PERF_PWR_OFFLOAD && HIF_PCI */

    return 0;
}
EXPORT_SYMBOL(ath_pci_resume);
#endif /* ATH_BUS_PM */

struct semaphore reset_in_progress;
bool driver_registered = false;
EXPORT_SYMBOL(reset_in_progress);
EXPORT_SYMBOL(driver_registered);

#if (ATH_PERF_PWR_OFFLOAD != 0) && defined(HIF_PCI)
/* peregrine */

void
ol_ath_pci_recovery_remove(struct pci_dev *pdev);

static void
ath_pci_recovery_remove(struct pci_dev *pdev)
{
    u_int8_t max_payload;

    pci_read_config_byte(pdev, 0x74, &max_payload);
#if (ATH_PERF_PWR_OFFLOAD != 0) && defined(HIF_PCI)
    if (((pdev->device == 0xabcd) && (max_payload != 0)) ||
        (pdev->device == AR9888_DEVICE_ID) ||
        (pdev->device == AR900B_DEVICE_ID) ||
        (pdev->device == AR9887_DEVICE_ID) ||
        (pdev->device == QCA9984_DEVICE_ID) ||
        (pdev->device == QCA9888_DEVICE_ID) ||
        (pdev->device == QCA6290_DEVICE_ID)) {
        ol_ath_pci_recovery_remove(pdev);
        return;
    }
#endif /* ATH_PERF_PWR_OFFLOAD && HIF_PCI */

}

int
ol_ath_pci_recovery_probe(struct pci_dev *pdev, const struct pci_device_id *id);

static int
ath_pci_recovery_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    u_int8_t max_payload;

    pci_read_config_byte(pdev, 0x74, &max_payload);
#if (ATH_PERF_PWR_OFFLOAD != 0) && defined(HIF_PCI)
    /* emulation device id, peregrine id or beeliner id*/
    if (((id->device == 0xabcd) && (max_payload != 0)) ||
        (id->device == AR9888_DEVICE_ID) ||
        (id->device == AR9887_DEVICE_ID) ||
        (id->device == AR900B_DEVICE_ID) ||
        (id->device == QCA9984_DEVICE_ID) ||
        (id->device == QCA9888_DEVICE_ID) ||
        (id->device == QCA6290_DEVICE_ID)) {
#if QCA_PARTNER_DIRECTLINK_TX
        wlan_pltfrm_directlink_set_pcie_base((uint32_t) pci_resource_start(pdev, 0));
#endif /* QCA_PARTNER_DIRECTLINK_TX */

        return ol_ath_pci_recovery_probe(pdev, id);
    }
#endif /* ATH_PERF_PWR_OFFLOAD && HIF_PCI */

    return (-ENODEV);
}


int ath_pci_recover(struct ol_ath_soc_softc *soc)
{
    struct _NIC_DEV *aps_osdev;

    aps_osdev = soc->sc_osdev;

    ath_pci_recovery_probe((struct pci_dev *)(aps_osdev->bdev), (const struct pci_device_id *)soc->platform_devid);
#if UMAC_SUPPORT_WEXT
    ol_ath_iw_attach(pci_get_drvdata(aps_osdev->bdev));
#endif

    return 0;
}

#if ACFG_NETLINK_TX
extern void acfg_offchan_cancel(struct ieee80211com *ic);
#endif
extern ol_ath_soc_softc_t *ol_global_soc[10];
void pci_defer_reconnect(void *context)
{
    struct pci_dev *pdev;
    const struct pci_device_id *id;
    struct ol_ath_soc_softc *soc = (struct ol_ath_soc_softc *)context;
    unsigned int soc_id;
    struct _NIC_DEV *aps_osdev;
    int recovery_enable;
    int ret = -1;
    struct pdev_op_args oper;
#if UMAC_SUPPORT_ACFG
    struct wlan_objmgr_pdev *pdev_obj;
    struct ieee80211com *ic;
#endif

#if defined(CONFIG_AR900B_SUPPORT) && defined(WLAN_FEATURE_BMI)
    if (soc->ol_if_ops->ramdump_handler)
	    soc->ol_if_ops->ramdump_handler(soc);
#endif

    if (soc->recovery_enable) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Resetting the radio\n");
        if (down_interruptible(&reset_in_progress))
            return;

        if (!driver_registered) {
            up(&reset_in_progress);
            return;
        }

        recovery_enable = soc->recovery_enable;
        /* We are here because the target has crashed
         * hence remove the PCI device and reload it
         */
        aps_osdev = soc->sc_osdev;
        pdev = aps_osdev->bdev;
        id =(const struct pci_device_id *)soc->platform_devid;
        soc_id = soc->soc_idx;

        rtnl_lock();
        ath_pci_recovery_remove(pdev);

        if (recovery_enable == RECOVERY_ENABLE_AUTO) {
            ret = ath_pci_recovery_probe(pdev, id);
            if(ret != 0) {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Recovery remove failed\n");
                rtnl_unlock();
#if UMAC_SUPPORT_ACFG
                pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_NB_ID);
                if (pdev_obj == NULL) {
                    qdf_print("%s: pdev object is NULL", __func__);
                }
                ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
                if (ic)
                    OSIF_RADIO_DELIVER_EVENT_WATCHDOG(ic, ACFG_WDT_INIT_FAILED);
                wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
#endif
                return;
            }

            /*Send event to user space */
            oper.type = PDEV_ITER_POWERUP;
            wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                       wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

            soc->recovery_in_progress = 0;
            soc->target_status = OL_TRGET_STATUS_CONNECTED;
            soc->soc_stats.tgt_asserts = 0;
        } else
#if UMAC_SUPPORT_WEXT
            ol_ath_iw_detach(pci_get_drvdata(pdev));
#endif

        /* The recovery process resets recovery_enable flag. restore it here */
        soc->recovery_enable = recovery_enable;

        rtnl_unlock();

        up(&reset_in_progress);

    }
    ;; //Do nothing
}
#endif  /* (ATH_PERF_PWR_OFFLOAD != 0) && defined(HIF_PCI) */

