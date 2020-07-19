/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
 */

#include <linux/pci.h>
#include <linux/err.h>
#include <linux/pci.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <qdf_trace.h>
#include <qdf_types.h>


#if !CONFIG_PLD_STUB

#ifdef CONFIG_PLD_PCIE_CNSS
#include <net/cnss2.h>
#endif

#include "pld_internal.h"
#include "pld_ahb.h"

#ifdef CONFIG_PCI

#ifdef QCA_WIFI_3_0_ADRASTEA
#define CE_COUNT_MAX 12
#else
#define CE_COUNT_MAX 8
#endif

/**
 * pld_ahb_probe() - Probe function for PCIE platform driver
 * @pdev: platform device
 * @id: platform device ID table
 *
 * The probe function will be called when platform device provided
 * in the ID table is detected.
 *
 * Return: int
 */
static int pld_ahb_probe(struct pci_dev *pcidev,
			  const struct pci_device_id *id)
{
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;
	int ret = 0;

	pld_context = pld_get_global_context();
	if (!pld_context) {
		ret = -ENODEV;
		goto out;
	}

	ret = pld_add_dev(pld_context, &pdev->dev, PLD_BUS_TYPE_AHB);
	if (ret)
		goto out;

	return pld_context->ops->probe(&pdev->dev,
		       PLD_BUS_TYPE_AHB, pdev, (void *)id);

out:
	return ret;
}

void *pld_ahb_subsystem_get(struct device *dev, int device_id)
{
	return cnss_subsystem_get(dev, device_id);
}

void pld_ahb_subsystem_put(struct device *dev)
{
	cnss_subsystem_put(dev);
}


/**
 * pld_ahb_remove() - Remove function for platform device
 * @pdev: platform device
 *
 * The remove function will be called when platform device is disconnected
 *
 * Return: void
 */
static void pld_ahb_remove(struct pci_dev *pcidev)
{
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();

	if (!pld_context)
		return;

	pld_context->ops->remove((struct device *)pdev, PLD_BUS_TYPE_AHB);

	pld_del_dev(pld_context, &pdev->dev);
}

#ifdef CONFIG_PLD_PCIE_CNSS
/**
 * pld_ahb_reinit() - SSR re-initialize function for platform device
 * @pdev: platform device
 * @id: platform device ID
 *
 * During subsystem restart(SSR), this function will be called to
 * re-initialize platform device.
 *
 * Return: int
 */
static int pld_ahb_reinit(struct pci_dev *pcidev,
			    const struct pci_device_id *id)
{
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (pld_context->ops->reinit)
		return pld_context->ops->reinit(&pdev->dev,
				PLD_BUS_TYPE_AHB, pdev, (void *)id);

	return -ENODEV;
}

/**
 * pld_ahb_fatal() - SSR fatal function for platform device
 * @pdev: platform device
 * @id: platform device ID
 *
 * Before subsystem restart(SSR), this function will be called
 * in atomic (IRQ) context to take quick action(disable tasklets)
 *
 * Return: int
 */
static int pld_ahb_fatal(struct pci_dev *pcidev,
			    const struct pci_device_id *id)
{
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (pld_context->ops->fatal)
		return pld_context->ops->fatal(&pdev->dev,
				PLD_BUS_TYPE_AHB, pdev, (void *)id);

	return -ENODEV;
}

/**
 * pld_ahb_shutdown() - SSR shutdown function for platform device
 * @pdev: platform device
 *
 * During SSR, this function will be called to shutdown platform device.
 *
 * Return: void
 */
static void pld_ahb_shutdown(struct pci_dev *pcidev)
{
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (pld_context->ops->shutdown)
		pld_context->ops->shutdown(&pdev->dev, PLD_BUS_TYPE_AHB);
}

/**
 * pld_ahb_crash_shutdown() - Crash shutdown function for platform device
 * @pdev: platform device
 *
 * This function will be called when a crash is detected, it will shutdown
 * the platform device.
 *
 * Return: void
 */
static void pld_ahb_crash_shutdown(struct pci_dev *pcidev)
{
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (pld_context->ops->crash_shutdown)
		pld_context->ops->crash_shutdown(&pdev->dev, PLD_BUS_TYPE_AHB);
}

/**
 * pld_ahb_notify_handler() - Modem state notification callback function
 * @pdev: platform device
 * @state: modem power state
 *
 * This function will be called when there's a modem power state change.
 *
 * Return: void
 */
static void pld_ahb_notify_handler(struct pci_dev *pcidev, int state)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (pld_context->ops->modem_status)
		pld_context->ops->modem_status(&pdev->dev,
					       PLD_BUS_TYPE_AHB, state);
#endif
}

/**
 * pld_ahb_update_status() - update wlan driver status callback function
 * @pdev: platform device
 * @status: driver status
 *
 * This function will be called when platform driver wants to update wlan
 * driver's status.
 *
 * Return: void
 */
static void pld_ahb_update_status(struct pci_dev *pcidev, uint32_t status,
				const struct pci_device_id *id)
{
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (pld_context->ops->update_status)
		pld_context->ops->update_status(&pdev->dev, status,
				PLD_BUS_TYPE_AHB, pdev, (void *)id);
}

#ifdef FEATURE_RUNTIME_PM
/**
 * pld_ahb_runtime_suspend() - PM runtime suspend
 * @pdev: platform device
 *
 * PM runtime suspend callback function.
 *
 * Return: int
 */
static int pld_ahb_runtime_suspend(struct pci_dev *pcidev)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (pld_context->ops->runtime_suspend)
		return pld_context->ops->runtime_suspend(&pdev->dev,
							 PLD_BUS_TYPE_AHB);
#endif

	return -ENODEV;
}

/**
 * pld_ahb_runtime_resume() - PM runtime resume
 * @pdev: platform device
 *
 * PM runtime resume callback function.
 *
 * Return: int
 */
static int pld_ahb_runtime_resume(struct pci_dev *pcidev)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (pld_context->ops->runtime_resume)
		return pld_context->ops->runtime_resume(&pdev->dev,
							PLD_BUS_TYPE_AHB);
#endif
	return -ENODEV;
}
#endif
#endif

#ifdef CONFIG_PM
#ifdef CONFIG_PLD_PCIE_CNSS
/**
 * pld_ahb_suspend() - Suspend callback function for power management
 * @pdev: platform device
 * @state: power state
 *
 * This function is to suspend the platform device when power management is
 * enabled.
 *
 * Return: void
 */
static int pld_ahb_suspend(struct pci_dev *pcidev, pm_message_t state)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	return pld_context->ops->suspend(&pdev->dev,
					 PLD_BUS_TYPE_AHB, state);
#endif
	return -EINVAL;
}

/**
 * pld_ahb_resume() - Resume callback function for power management
 * @pdev: platform device
 *
 * This function is to resume the platform device when power management is
 * enabled.
 *
 * Return: void
 */
static int pld_ahb_resume(struct pci_dev *pcidev)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	return pld_context->ops->resume(&pdev->dev, PLD_BUS_TYPE_AHB);
#endif
	return -EINVAL;
}

/**
 * pld_ahb_suspend_noirq() - Complete the actions started by suspend()
 * @pdev: PCI device
 *
 * Complete the actions started by suspend().  Carry out any additional
 * operations required for suspending the device that might be racing
 * with its driver's interrupt handler, which is guaranteed not to run
 * while suspend_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_ahb_suspend_noirq(struct pci_dev *pcidev)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->suspend_noirq)
		return pld_context->ops->
			suspend_noirq(&pdev->dev, PLD_BUS_TYPE_AHB);
	return 0;
#endif
	return -EINVAL;
}

/**
 * pld_ahb_resume_noirq() - Prepare for the execution of resume()
 * @pdev: PCI device
 *
 * Prepare for the execution of resume() by carrying out any additional
 * operations required for resuming the device that might be racing with
 * its driver's interrupt handler, which is guaranteed not to run while
 * resume_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_ahb_resume_noirq(struct pci_dev *pcidev)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->resume_noirq)
		return pld_context->ops->
			resume_noirq(&pdev->dev, PLD_BUS_TYPE_AHB);
	return 0;
#endif
	return -EINVAL;
}
#else
/**
 * pld_ahb_pm_suspend() - Suspend callback function for power management
 * @dev: device
 *
 * This function is to suspend the platform device when power management is
 * enabled.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_ahb_pm_suspend(struct device *dev)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pm_message_t state = { .event = PM_EVENT_SUSPEND };

	pld_context = pld_get_global_context();
	return pld_context->ops->suspend(dev, PLD_BUS_TYPE_AHB, state);
#endif
	return -EINVAL;
}

/**
 * pld_ahb_pm_resume() - Resume callback function for power management
 * @dev: device
 *
 * This function is to resume the platform device when power management is
 * enabled.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_ahb_pm_resume(struct device *dev)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	return pld_context->ops->resume(dev, PLD_BUS_TYPE_AHB);
#endif
	return -EINVAL;
}

/**
 * pld_ahb_pm_suspend_noirq() - Complete the actions started by suspend()
 * @dev: device
 *
 * Complete the actions started by suspend().  Carry out any additional
 * operations required for suspending the device that might be racing
 * with its driver's interrupt handler, which is guaranteed not to run
 * while suspend_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_ahb_pm_suspend_noirq(struct device *dev)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->suspend_noirq)
		return pld_context->ops->suspend_noirq(dev, PLD_BUS_TYPE_AHB);
#endif
	return 0;
}

/**
 * pld_ahb_pm_resume_noirq() - Prepare for the execution of resume()
 * @dev: device
 *
 * Prepare for the execution of resume() by carrying out any additional
 * operations required for resuming the device that might be racing with
 * its driver's interrupt handler, which is guaranteed not to run while
 * resume_noirq() is being executed.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
static int pld_ahb_pm_resume_noirq(struct device *dev)
{
#if NOT_DEFINED
	struct pld_context *pld_context;
	struct platform_device *pdev = (struct platform_device *)pcidev;

	pld_context = pld_get_global_context();
	if (!pld_context)
		return -EINVAL;

	if (pld_context->ops->resume_noirq)
		return pld_context->ops->
			resume_noirq(dev, PLD_BUS_TYPE_AHB);
#endif
	return 0;
}
#endif
#endif

static struct pci_device_id pld_ahb_id_table[] = {
	{ 0x168c, 0xffff, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};

#ifdef CONFIG_PLD_PCIE_CNSS
#ifdef FEATURE_RUNTIME_PM
struct cnss_wlan_runtime_ops runtime_pm_ops = {
	.runtime_suspend = pld_pcie_runtime_suspend,
	.runtime_resume = pld_ahb_runtime_resume,
};
#endif

struct cnss_wlan_driver pld_ahb_ops = {
	.name       = "pld_ahb",
	.id_table   = pld_ahb_id_table,
	.probe      = pld_ahb_probe,
	.remove     = pld_ahb_remove,
	.reinit     = pld_ahb_reinit,
	.shutdown   = pld_ahb_shutdown,
	.crash_shutdown = pld_ahb_crash_shutdown,
	.modem_status   = pld_ahb_notify_handler,
	.update_status  = pld_ahb_update_status,
#ifdef CONFIG_PM
	.suspend    = pld_ahb_suspend,
	.resume     = pld_ahb_resume,
	.suspend_noirq = pld_ahb_suspend_noirq,
	.resume_noirq  = pld_ahb_resume_noirq,
#endif
#ifdef FEATURE_RUNTIME_PM
	.runtime_ops = &runtime_pm_ops,
#endif
	.fatal		= pld_ahb_fatal,
};

/**
 * pld_ahb_register_driver() - Register platform device callback functions
 *
 * Return: int
 */
int pld_ahb_register_driver(void)
{
	return cnss_wlan_register_driver(&pld_ahb_ops);
}

/**
 * pld_ahb_unregister_driver() - Unregister platform device callback functions
 *
 * Return: void
 */
void pld_ahb_unregister_driver(void)
{
	cnss_wlan_unregister_driver(&pld_ahb_ops);
}
#else
static const struct platform_device_id ath9k_platform_id_table[] = {
	{
		.name = "ipq40xx_wmac", /* Update the name with proper device name (ipqxxxx) */
		.driver_data = IPQ4019_DEVICE_ID,
    },
#ifdef QCA_WIFI_QCA8074
	{
		.name = "ipq807x_wmac", /* Update the name with proper device name (ipqxxxx) */
		.driver_data = QCA8074_DEVICE_ID,
	},
#endif
#ifdef QCA_WIFI_QCA8074V2
	{
		.name = "ipq807xv2_wmac", /* Update the name with proper device name (ipqxxxx) */
		.driver_data = QCA8074V2_DEVICE_ID,
	},
#endif
#ifdef QCA_WIFI_QCA6018
	{
		.name = "ipq60xx_wmac", /* Update the name with proper device name (ipqxxxx) */
		.driver_data = QCA6018_DEVICE_ID,
	},
#endif
    {},
};

#ifdef CONFIG_OF
/* Update the name with proper device name (wifi-qcaxxxx) */
const struct of_device_id ath_wifi_of_match[] = {
    {.compatible = "qca,wifi-ipq40xx", .data = (void *) &ath9k_platform_id_table[0] },
#ifdef QCA_WIFI_QCA8074
    {.compatible = "qca,wifi-ipq807x", .data = (void *) &ath9k_platform_id_table[1] },
#endif
    { /*sentinel*/},
};
#endif

static struct platform_driver ath_ahb_driver = {
	.probe      = ath_ahb_probe,
	.remove     = ath_ahb_remove,
	.driver		= {
		.name	= "ath10k_ahb",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = ath_wifi_of_match,
#endif
	},
	.id_table    = ath9k_platform_id_table,
};

MODULE_DEVICE_TABLE(platform, ath9k_platform_id_table);

int pld_ahb_register_driver(void)
{
	return platform_driver_register(&pld_ahb_ops);
}

void pld_ahb_unregister_driver(void)
{
	platform_driver_register(&pld_ahb_ops);
}
#endif

/**
 * pld_ahb_get_ce_id() - Get CE number for the provided IRQ
 * @irq: IRQ number
 *
 * Return: CE number
 */
int pld_ahb_get_ce_id(int irq)
{
	int ce_id = irq - 100;
	if (ce_id < CE_COUNT_MAX && ce_id >= 0)
		return ce_id;

	return -EINVAL;
}

#ifdef CONFIG_PLD_PCIE_CNSS
/**
 * pld_ahb_wlan_enable() - Enable WLAN
 * @config: WLAN configuration data
 * @mode: WLAN mode
 * @host_version: host software version
 *
 * This function enables WLAN FW. It passed WLAN configuration data,
 * WLAN mode and host software version to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_ahb_wlan_enable(struct device *dev, struct pld_wlan_enable_cfg *config,
			 enum pld_driver_mode mode, const char *host_version)
{
	struct cnss_wlan_enable_cfg cfg;
	enum cnss_driver_mode cnss_mode;

	memset(&cfg, 0, sizeof(cfg));

	if (!config)
		goto skip_cfg;

	cfg.num_ce_tgt_cfg = config->num_ce_tgt_cfg;
	cfg.ce_tgt_cfg = (struct cnss_ce_tgt_pipe_cfg *)
		config->ce_tgt_cfg;
	cfg.num_ce_svc_pipe_cfg = config->num_ce_svc_pipe_cfg;
	cfg.ce_svc_cfg = (struct cnss_ce_svc_pipe_cfg *)
		config->ce_svc_cfg;
#ifdef CONFIG_CNSS2_SUPPORT
	cfg.num_shadow_reg_cfg = config->num_shadow_reg_cfg;
	cfg.shadow_reg_cfg = (struct cnss_shadow_reg_cfg *)
		config->shadow_reg_cfg;
	cfg.num_shadow_reg_v2_cfg = config->num_shadow_reg_v2_cfg;
	cfg.shadow_reg_v2_cfg = (struct cnss_shadow_reg_v2_cfg *)
		config->shadow_reg_v2_cfg;
#endif

skip_cfg:

	switch (mode) {
	case PLD_FTM:
		cnss_mode = CNSS_FTM;
		break;
	case PLD_WALTEST:
		cnss_mode = CNSS_WALTEST;
		break;
	case PLD_EPPING:
		cnss_mode = CNSS_EPPING;
		break;
        case PLD_COLDBOOT_CALIBRATION:
		cnss_mode = CNSS_CALIBRATION;
		qdf_print("cnss_mode: Cold boot calibration");
		break;
	default:
		cnss_mode = CNSS_MISSION;
		qdf_print("cnss_mode: Mission mode ");
		break;
	}
	return cnss_wlan_enable(dev, &cfg, cnss_mode, host_version);
}

/**
 * pld_ahb_wlan_disable() - Disable WLAN
 * @mode: WLAN mode
 *
 * This function disables WLAN FW. It passes WLAN mode to FW.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_ahb_wlan_disable(struct device *dev, enum pld_driver_mode mode)
{
	return cnss_wlan_disable(dev, CNSS_OFF);
}

/**
 * pld_ahb_get_fw_files_for_target() - Get FW file names
 * @pfw_files: buffer for FW file names
 * @target_type: target type
 * @target_version: target version
 *
 * Return target specific FW file names to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_ahb_get_fw_files_for_target(struct device *dev, struct pld_fw_files *pfw_files,
				     u32 target_type, u32 target_version)
{
	int ret = 0;
	struct cnss_fw_files cnss_fw_files;

	if (pfw_files == NULL)
		return -ENODEV;

	memset(pfw_files, 0, sizeof(*pfw_files));

	ret = cnss_get_fw_files_for_target(dev, &cnss_fw_files,
					   target_type, target_version);
	if (0 != ret)
		return ret;

	strlcpy(pfw_files->image_file, cnss_fw_files.image_file,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->board_data, cnss_fw_files.board_data,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->otp_data, cnss_fw_files.otp_data,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->utf_file, cnss_fw_files.utf_file,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->utf_board_data, cnss_fw_files.utf_board_data,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->epping_file, cnss_fw_files.epping_file,
		PLD_MAX_FILE_NAME);
	strlcpy(pfw_files->evicted_data, cnss_fw_files.evicted_data,
		PLD_MAX_FILE_NAME);

	return 0;
}

/**
 * pld_ahb_get_platform_cap() - Get platform capabilities
 * @cap: buffer to the capabilities
 *
 * Return capabilities to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_ahb_get_platform_cap(struct device *dev, struct pld_platform_cap *cap)
{
	int ret = 0;
	struct cnss_platform_cap cnss_cap;

	if (cap == NULL)
		return -ENODEV;

	ret = cnss_get_platform_cap(dev, &cnss_cap);
	if (0 != ret)
		return ret;

	memcpy(cap, &cnss_cap, sizeof(*cap));
	return 0;
}

/**
 * pld_ahb_get_soc_info() - Get SOC information
 * @info: buffer to SOC information
 *
 * Return SOC info to the buffer.
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_ahb_get_soc_info(struct device *dev, struct pld_soc_info *info)
{
	int ret = 0;
	struct cnss_soc_info cnss_info;

	if (info == NULL)
		return -ENODEV;

	ret = cnss_get_soc_info(dev, &cnss_info);
	if (ret)
		return ret;

	memcpy(info, &cnss_info, sizeof(*info));

	return 0;
}

/**
 * pld_ahb_set_driver_status() - Set driver status
 * @status: driver status
 *
 * Return: void
 */
void pld_ahb_set_driver_status(enum pld_driver_status status)
{
	enum cnss_driver_status cnss_status;

	switch (status) {
	case PLD_UNINITIALIZED:
		cnss_status = CNSS_UNINITIALIZED;
		break;
	case PLD_INITIALIZED:
		cnss_status = CNSS_INITIALIZED;
		break;
	default:
		cnss_status = CNSS_LOAD_UNLOAD;
		break;
	}
}

/**
 * pld_ahb_schedule_recovery_work() - schedule recovery work
 * @dev: device
 * @reason: recovery reason
 *
 * Return: void
 */
void pld_ahb_schedule_recovery_work(struct device *dev,
				     enum pld_recovery_reason reason)
{
	enum cnss_recovery_reason cnss_reason;

	switch (reason) {
	case PLD_REASON_LINK_DOWN:
		cnss_reason = CNSS_REASON_LINK_DOWN;
		break;
	default:
		cnss_reason = CNSS_REASON_DEFAULT;
		break;
	}
	cnss_schedule_recovery(dev, cnss_reason);
}

/**
 * pld_ahb_device_self_recovery() - device self recovery
 * @dev: device
 * @reason: recovery reason
 *
 * Return: void
 */
void pld_ahb_device_self_recovery(struct device *dev,
				   enum pld_recovery_reason reason)
{
	enum cnss_recovery_reason cnss_reason;

	switch (reason) {
	case PLD_REASON_LINK_DOWN:
		cnss_reason = CNSS_REASON_LINK_DOWN;
		break;
	default:
		cnss_reason = CNSS_REASON_DEFAULT;
		break;
	}
	cnss_self_recovery(dev, cnss_reason);
}
#endif
#endif
#endif
