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

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <acfg_api_types.h>   /* for ACFG_WDT_REINIT_DONE */
#include <acfg_drv_event.h>
#include <hif.h>

#include <pld_common.h>
#include "qal_vbus_dev.h"

extern unsigned int ahbskip;
#ifdef USE_PLATFORM_FRAMEWORK
static struct platform_device *spdev;
#endif /* USE_PLATFORM_FRAMEWORK */

#if UMAC_SUPPORT_WEXT
void ol_ath_iw_detach(struct net_device *dev);
void ol_ath_iw_attach(struct net_device *dev);
#endif
struct net_device *platform_dev_to_net_dev(struct platform_device *pdev);
void ol_if_register_wifi2_0(void);
void wmi_non_tlv_init(void);
void ce_service_legacy_init(void);

#ifdef USE_PLATFORM_FRAMEWORK
int
get_wmac_irq(u_int16_t wmac_num)
{
	int ret;
	struct resource *res = NULL;
        struct qdf_vbus_resource *rsrc = NULL;

	qal_vbus_get_resource((struct qdf_pfm_hndl *)spdev, &rsrc,
			      IORESOURCE_IRQ, 0);
	res = (struct resource *)rsrc;
	if (res == NULL) {
		printk("no IRQ resource found\n");
		ret = -ENXIO;
		goto out;
	}

	ret = res->start;
out:
	return ret;
}

unsigned long
get_wmac_base(u_int16_t wmac_num)
{
	void __iomem *mem = NULL;
	struct resource *res = NULL;
	unsigned long ret;
        struct qdf_vbus_resource *rsrc = NULL;

	qal_vbus_get_resource((struct qdf_pfm_hndl *)spdev, &rsrc,
			      IORESOURCE_MEM, 0);
	res =(struct resource *)rsrc;
	if (res == NULL) {
		printk("no memory resource found\n");
		ret = -ENXIO;
		goto err_out;
	}

	mem = ioremap_nocache(res->start, resource_size(res));
	if (mem == NULL) {
		printk("ioremap failed\n");
		ret = -ENOMEM;
		goto err_out;
	}

	return (unsigned long) mem;
err_out:
	iounmap(mem);
	return ret;
}

unsigned long
get_wmac_mem_len(u_int16_t wmac_num)
{
	struct resource *res = NULL;
	unsigned long ret;
        struct qdf_vbus_resource *rsrc = NULL;

	qal_vbus_get_resource((struct qdf_pfm_hndl *)spdev, &rsrc,
			      IORESOURCE_MEM, 0);
	res = (struct resource *)rsrc;
	if (res == NULL) {
		dev_err(&spdev->dev, "no memory resource found\n");
		ret = -ENXIO;
		goto out;
	}
	ret = resource_size(res);
out:
	return ret;
}
#endif /* USE_PLATFORM_FRAMEWORK */

static int
init_ath_wmac(u_int16_t devid, u_int16_t wlanNum)
{
    return 0;
}

static int
exit_ath_wmac(u_int16_t wlanNum)
{
    return 0;
}

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

#if !defined(USE_PLATFORM_FRAMEWORK)

#else
static const struct platform_device_id ath10k_wifi2_platform_id_table[] = {
	{
		.name = "ipq40xx_wmac", /* Update the name with proper device name (ipqxxxx) */
		.driver_data = IPQ4019_DEVICE_ID,
    },
    {},
};

#ifdef CONFIG_OF
/* Update the name with proper device name (wifi-qcaxxxx) */
const struct of_device_id ath_wifi_of_match[] = {
    {.compatible = "qca,wifi-ipq40xx", .data = (void *) &ath10k_wifi2_platform_id_table[0] },
    { /*sentinel*/},
};

const struct of_device_id cnss_wifi_of_match[] = {
    { /*sentinel*/},
};
#endif

static int ath_ahb_probe(struct platform_device *pdev)
{
	const struct platform_device_id *id;
	int ret;
#ifdef CONFIG_OF
    const struct of_device_id *of_id = NULL;
#endif

#ifdef CONFIG_OF
    of_id = of_match_device(ath_wifi_of_match, &pdev->dev);
    if (of_id) {
        id = of_id->data;
        if (ol_ath_supported_dev(id)) {
            return ol_ath_ahb_probe(pdev, id);
        }
    } else
#endif
    id = platform_get_device_id(pdev);

    /* Do not register Direct attach devices if ahbskip is 1 */
    if(ahbskip) {
        ret = -EINVAL;
        goto out;
    }

	spdev = pdev;

    /* Assuming platform_data will be populated in case of non OF devices */
#ifdef CONFIG_OF
    if (!of_id)
    {
#endif
        if (!pdev->dev.platform_data) {
            printk("no platform data specified\n");
            ret = -EINVAL;
            goto out;
        }
#ifdef CONFIG_OF
    }
#endif

	ret = init_ath_wmac(id->driver_data, 0);
out:
	return ret;
}

const struct platform_device_id *ath_ahb_get_pdev_id_2_0(struct platform_device *pdev)
{
#ifdef CONFIG_OF
    const struct of_device_id *of_id = NULL;
    const struct platform_device_id *id;

    of_id = of_match_device(cnss_wifi_of_match, &pdev->dev);
    if(!of_id) {
            return NULL;
    }

    id = of_id->data;
    return id;
#else
    return NULL;
#endif
}
EXPORT_SYMBOL(ath_ahb_get_pdev_id_2_0);

static int ath_ahb_remove(struct platform_device *pdev)
{
#ifdef CONFIG_OF
    const struct of_device_id *of_id = NULL;
    const struct platform_device_id *id;
#endif

	spdev = NULL;

#ifdef CONFIG_OF
    of_id = of_match_device(ath_wifi_of_match, &pdev->dev);
    if (of_id) {
        id = of_id->data;
        if (ol_ath_supported_dev(id)) {
            ol_ath_ahb_remove(pdev);
            return 0;
        }
    }
#endif
	exit_ath_wmac(0);
	return 0;
}

int
ol_ath_ahb_recovery_probe(struct platform_device *pdev, const struct platform_device_id *id);

static int
ath_ahb_recovery_probe(struct platform_device *pdev, const struct platform_device_id *pdid)
{
#ifdef CONFIG_OF
    const struct of_device_id *of_id = NULL;
    const struct platform_device_id *id;
#endif

#ifdef CONFIG_OF
    of_id = of_match_device(ath_wifi_of_match, &pdev->dev);
    if (of_id) {
        id = of_id->data;
        if (id->driver_data == IPQ4019_DEVICE_ID) {
            ol_ath_ahb_recovery_probe(pdev, id);
            return 0;
        }
    }
#endif

	return 0;
}

int ath_ahb_recover_2_0(struct ol_ath_soc_softc *soc)
{
    struct _NIC_DEV *aps_osdev;

    aps_osdev = soc->sc_osdev;

    ath_ahb_recovery_probe((struct platform_device *)(aps_osdev->bdev), NULL);

#if UMAC_SUPPORT_WEXT
    ol_ath_iw_attach(platform_dev_to_net_dev(aps_osdev->bdev));
#endif

    return 0;
}
EXPORT_SYMBOL(ath_ahb_recover_2_0);

void
ol_ath_ahb_recovery_remove(struct platform_device *pdev);

static void
ath_ahb_recovery_remove(struct platform_device *pdev)
{
#ifdef CONFIG_OF
    const struct of_device_id *of_id = NULL;
    const struct platform_device_id *id;

    of_id = of_match_device(ath_wifi_of_match, &pdev->dev);
    if (of_id) {
        id = of_id->data;
        if (id->driver_data == IPQ4019_DEVICE_ID) {
            ol_ath_ahb_recovery_remove(pdev);
            return ;
        }
    }
#endif

	return ;
}

void ahb_defer_reconnect_2_0(void *context)
{
    struct platform_device *pdev;
    struct ol_ath_soc_softc *soc = (struct ol_ath_soc_softc *)context;
    struct _NIC_DEV *aps_osdev;
    int recovery_enable;
    struct pdev_op_args oper;

#if defined(CONFIG_AR900B_SUPPORT) && defined(WLAN_FEATURE_BMI)
    if (soc->ol_if_ops->ramdump_handler)
        soc->ol_if_ops->ramdump_handler(soc);
#endif

    if (soc->recovery_enable) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Resetting the radio\n");
        recovery_enable = soc->recovery_enable;
        /* We are here because the target has crashed

         * hence remove the AHB device and reload it
         */
        aps_osdev = soc->sc_osdev;
        pdev = aps_osdev->bdev;

        rtnl_lock();

	ath_ahb_recovery_remove(pdev);

        if (recovery_enable == RECOVERY_ENABLE_AUTO) {
            ath_ahb_recovery_probe(pdev, NULL);

            /*Send event to user space */
            oper.type = PDEV_ITER_POWERUP;
            wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                       wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

            soc->recovery_in_progress = 0;
            soc->target_status = OL_TRGET_STATUS_CONNECTED;
        } else
#if UMAC_SUPPORT_WEXT
            ol_ath_iw_detach(platform_dev_to_net_dev(pdev));
#endif

        /* The recovery process resets recovery_enable flag. restore again here */
        soc->recovery_enable = recovery_enable;
        soc->soc_stats.tgt_asserts = 0;

        rtnl_unlock();

    }
}
EXPORT_SYMBOL(ahb_defer_reconnect_2_0);

static struct platform_driver ath_ahb_driver = {
	.probe      = ath_ahb_probe,
	.remove     = ath_ahb_remove,
	.driver		= {
		.name	= "ath10k_wifi2_ahb",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
        .of_match_table = ath_wifi_of_match,
#endif
	},
	.id_table    = ath10k_wifi2_platform_id_table,
};

MODULE_DEVICE_TABLE(platform, ath10k_wifi2_platform_id_table);

extern struct nss_wifi_soc_ops nss_wifibe_soc_ops;
extern void osif_nss_register_module(OL_WIFI_DEV_TYPE target_type,
					struct nss_wifi_soc_ops *soc_ops);

int init_ath_ahb_2_0(void)
{
	int ret = 0;

#ifdef WIFI_TARGET_TYPE_2_0
	qdf_debug("WIFI2.0 Registration");
	/* register wifi2.0 architecture ops */
	ol_if_register_wifi2_0();
	wmi_non_tlv_init();
	ce_service_legacy_init();
#if QCA_NSS_WIFI_2_0_OFFLOAD_SUPPORT
	osif_nss_register_module(OL_WIFI_2_0, &nss_wifibe_soc_ops);
#endif
#endif

	ret = platform_driver_register(&ath_ahb_driver);
	if(ret) {
		qdf_print("platform_driver_register failed %d",ret);
		return ret;
	}

	return 0;
}

void exit_ath_ahb_2_0(void)
{
	platform_driver_unregister(&ath_ahb_driver);
}
#endif

#if !defined(ATH_PCI)
module_init(init_ath_ahb_2_0);
module_exit(exit_ath_ahb_2_0);
#endif /* ATH_PCI */
