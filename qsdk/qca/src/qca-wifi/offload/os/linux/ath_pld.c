/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */
#include <linux/version.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include "pld_common.h"
#include <qdf_types.h>
#include <qdf_trace.h>
#include <qdf_util.h>
#include <soc/qcom/subsystem_notif.h>
int ol_ath_ahb_ssr(struct platform_device *pdev,
				struct platform_device_id *id,
				  unsigned long code);

extern int ol_ath_pci_probe(struct pci_dev *pdev,
                        const struct pci_device_id *id);
extern void ol_ath_pci_remove(struct pci_dev *pdev);

enum qdf_bus_type to_bus_type(enum pld_bus_type bus_type)
{
        switch (bus_type) {
        case PLD_BUS_TYPE_PCIE:
                return QDF_BUS_TYPE_PCI;
        case PLD_BUS_TYPE_SNOC:
                return QDF_BUS_TYPE_SNOC;
        case PLD_BUS_TYPE_SDIO:
                return QDF_BUS_TYPE_SDIO;
        case PLD_BUS_TYPE_USB:
                return QDF_BUS_TYPE_USB;
	case PLD_BUS_TYPE_AHB:
		return QDF_BUS_TYPE_AHB;
        default:
                return QDF_BUS_TYPE_NONE;
        }
}

int ol_ath_pld_probe(struct device *dev,enum pld_bus_type bus_type,void *bdev,
                                                void *id)
{
	int ret = 0;
	if (bus_type == PLD_BUS_TYPE_PCIE)  {
		ret = ol_ath_pci_probe((struct pci_dev *)bdev,
                                (struct pci_device_id *)id);
	}
	else {
		ret = ol_ath_ahb_ssr((struct platform_device *) bdev,
                        (struct platform_device_id *)id, SUBSYS_AFTER_POWERUP);
	}
	return ret;
}
void ol_ath_pld_remove(struct device *dev,enum pld_bus_type bus_type)
{
	void *pdev = (void *)dev;
	if (bus_type == PLD_BUS_TYPE_PCIE) {
		ol_ath_pci_remove((struct pci_dev *)pdev);
	}
	else
		ol_ath_ahb_ssr((struct platform_device *)pdev, NULL,
                                SUBSYS_BEFORE_SHUTDOWN);
}

static void ol_ath_pld_shutdown(struct device *dev,
                       enum pld_bus_type bus_type)
{
}

static int ol_ath_pld_fatal(struct device *dev,
                    enum pld_bus_type pld_bus_type,
		                        void *bdev, void *id)
{
	enum qdf_bus_type bus_type;
	int ret = 0;
	struct pci_dev *pdev =(struct pci_dev *)bdev;
	struct pci_device_id *id1 = (struct pci_device_id *)id;
	bus_type = to_bus_type(pld_bus_type);
	if (bus_type == QDF_BUS_TYPE_NONE) {
		qdf_print("Invalid bus type %d->%d",
				pld_bus_type, bus_type);
		return -EINVAL;
	}
	if (bus_type != QDF_BUS_TYPE_AHB)  {
        /* TODO: non-AHB (PCI) handling */
	}
	else {
		ret = ol_ath_ahb_ssr((struct platform_device *)pdev,
						(struct platform_device_id *) id1,
						SUBSYS_PREPARE_FOR_FATAL_SHUTDOWN);
	}
	return ret;
}

static int ol_ath_pld_reinit(struct device *dev,
                    enum pld_bus_type pld_bus_type,
		                        void *bdev, void *id)
{
	enum qdf_bus_type bus_type;
	int ret = 0;
	struct pci_dev *pdev =(struct pci_dev *)bdev;
	struct pci_device_id *id1 = (struct pci_device_id *)id;
	bus_type = to_bus_type(pld_bus_type);
	if (bus_type == QDF_BUS_TYPE_NONE) {
		qdf_print("Invalid bus type %d->%d",
				pld_bus_type, bus_type);
		return -EINVAL;
	}
	if (bus_type != QDF_BUS_TYPE_AHB)  {
		ret = ol_ath_pci_probe(pdev, id1);
	}
	else {
		ret = ol_ath_ahb_ssr((struct platform_device *)pdev,
                        (struct platform_device_id *) id1,
                        SUBSYS_RAMDUMP_NOTIFICATION);
	}
	return ret;
}

int ol_ath_pld_suspend(struct device *dev, enum pld_bus_type bus_type,
                        pm_message_t state)
{
	return 0;
}

int ol_ath_pld_resume(struct device *dev, enum pld_bus_type bus_type)
{
        return 0;
}

static int ol_ath_pld_suspend_noirq(struct device *dev,
                     enum pld_bus_type bus_type)
{
        return 0;
}
static int ol_ath_pld_resume_noirq(struct device *dev,
                    enum pld_bus_type bus_type)
{

        return 0;
}
static int ol_ath_pld_reset_resume(struct device *dev,
                    enum pld_bus_type bus_type)
{
        return 0;
}
static void ol_ath_pld_notify_handler(struct device *dev,
                             enum pld_bus_type bus_type,
			                                  int state)
{
}
static void ol_ath_pld_update_status(struct device *dev, uint32_t status,
			enum pld_bus_type bus_type, void *bdev, void *id)
{
	int ret = 0;
        switch (bus_type) {
        case PLD_BUS_TYPE_AHB:
		ret = ol_ath_ahb_ssr((struct platform_device *) bdev,
                        (struct platform_device_id *)id, status);
		break;
        default:
		break;
	}
}
static void ol_ath_pld_crash_shutdown(struct device *dev,
                enum pld_bus_type bus_type)
{
}
#ifdef FEATURE_RUNTIME_PM
static int ol_ath_pld_runtime_suspend(struct device *dev,
                                        enum pld_bus_type bus_type)
{
        return 0;
}
static int ol_ath_pld_runtime_resume(struct device *dev,
                                       enum pld_bus_type bus_type)
{
        return 0;
}
#endif

struct pld_driver_ops wlan_drv_ops = {
        .probe      = ol_ath_pld_probe,
        .remove     = ol_ath_pld_remove,
        .shutdown   = ol_ath_pld_shutdown,
        .reinit     = ol_ath_pld_reinit,
        .crash_shutdown = ol_ath_pld_crash_shutdown,
        .suspend    = ol_ath_pld_suspend,
        .resume     = ol_ath_pld_resume,
        .suspend_noirq = ol_ath_pld_suspend_noirq,
        .resume_noirq  = ol_ath_pld_resume_noirq,
        .reset_resume = ol_ath_pld_reset_resume,
        .modem_status = ol_ath_pld_notify_handler,
        .update_status = ol_ath_pld_update_status,
#ifdef FEATURE_RUNTIME_PM
        .runtime_suspend = ol_ath_pld_runtime_suspend,
        .runtime_resume = ol_ath_pld_runtime_resume,
#endif
        .fatal          = ol_ath_pld_fatal,
};


int pld_module_init(void)
{
        return pld_register_driver(&wlan_drv_ops);
}
EXPORT_SYMBOL(pld_module_init);


void pld_module_exit(void)
{
	pld_unregister_driver();
}
EXPORT_SYMBOL(pld_module_exit);
