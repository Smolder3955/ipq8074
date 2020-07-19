/*
 * Copyright (c) 2013,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __ATH_PCI_H__
#define __ATH_PCI_H__

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#if DUMP_FW_RAM
#if ATH_SUPPORT_FW_RAM_DUMP_FOR_MIPS
#include <linux/ath79_wlan.h>
#endif
#endif

/* Maximum amount of time in micro seconds before which the CE per engine service
 * should yield. ~1 jiffie.
 */
#define CE_PER_ENGINE_SERVICE_MAX_YIELD_TIME (4 * 1000)

void ol_hif_close(void *hif_ctx);
int
ol_ath_ahb_probe(struct platform_device *pdev, const struct platform_device_id *id);
void
ol_ath_ahb_remove(struct platform_device *pdev);
extern void pci_defer_reconnect(void *pci_reconnect_work);
void pci_reconnect_cb(ol_ath_soc_softc_t *soc);
extern bool ol_ath_supported_dev(const struct platform_device_id *id);
extern uint32_t ol_ath_get_hw_mode_id(void *bdev);
int ath_pci_recover(struct ol_ath_soc_softc *soc);
int ath_ahb_recover(struct ol_ath_soc_softc *soc);
#endif /* __ATH_PCI_H__ */
