/*
 * Copyright (c) 2015,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <hif.h>
#include <osdep.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/if_arp.h>
#include "ath_pci.h"
#include "ol_ath.h"
#include "hif.h"
#include "hif_napi.h"
#include "pld_common.h"
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include "ah_devid.h"
#include <qdf_mem.h>
#include <osif_private.h>
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#endif
#if CONFIG_LEDS_IPQ
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
#include <drivers/leds/leds-ipq.h>
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
#include <drivers/leds/leds-ipq40xx.h>
#endif
#endif
#ifdef CONFIG_CNSS2_SUPPORT
#include <soc/qcom/subsystem_notif.h>
#include <net/cnss2.h>
#endif
#include <init_deinit_lmac.h>
#include <hif_main.h>
#include "qal_vbus_dev.h"


unsigned int soc_probe_module_load = 1;
module_param(soc_probe_module_load, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(soc_probe_module_load,
                       "Do soc_module_load Configuration");
EXPORT_SYMBOL(soc_probe_module_load);
extern struct platform_device_id *ath_ahb_get_pdev_id(struct platform_device *pdev);
struct pld_plat_data  *wlan_sc, *cnss_plat;
extern ol_ath_soc_softc_t *ol_global_soc[GLOBAL_SOC_SIZE];
extern int ol_num_global_soc;
extern void cnss_dump_qmi_history(void);

void *ol_hif_open(struct device *dev, void *bdev, void *bid,
        enum qdf_bus_type bus_type, bool reinit, qdf_device_t qdf_dev);
void ol_hif_close(void *hif_ctx);
int ol_ath_soc_start(struct ol_ath_soc_softc *soc);
int ol_ath_soc_stop(struct ol_ath_soc_softc *soc, bool flush_wq);
int ol_ath_target_start(struct ol_ath_soc_softc *scn);
int osif_recover_vap_delete(struct ieee80211com *ic);
void ath_vdev_recover_delete(struct wlan_objmgr_psoc *psoc, void *obj, void *args);
void ol_ath_tx_stop(struct ol_ath_soc_softc *soc);
void ol_ath_target_timer_stop(struct ol_ath_soc_softc *soc);
void ol_ath_ahb_recovery_remove(struct platform_device *pdev);
int ol_ath_ahb_recovery_probe(struct platform_device *pdev, const struct platform_device_id *id);
void hif_set_target_status(struct hif_opaque_softc *hif_ctx, enum
                           hif_target_status status);
#if UMAC_SUPPORT_WEXT
void ol_ath_iw_attach(struct net_device *dev);
void ol_ath_iw_detach(struct net_device *dev);
#endif

struct net_device *platform_dev_to_net_dev(struct platform_device *pdev)
{
    struct pld_plat_data *cnss_plat, *wlan_sc;
    struct net_device *dev = NULL;
    struct platform_device_id *id = NULL;

    if(!pdev) {
            return NULL;
    }

    id = ath_ahb_get_pdev_id(pdev);

    if ((id && id->driver_data == QCA8074_DEVICE_ID) ||
        (id && id->driver_data == QCA8074V2_DEVICE_ID) ||
        (id && id->driver_data == QCA6018_DEVICE_ID)) {
            cnss_plat = platform_get_drvdata(pdev);
            wlan_sc  = cnss_plat->wlan_priv;
            if(wlan_sc)
	            dev = (struct net_device *) wlan_sc->wlan_priv;
    } else {
            dev = platform_get_drvdata(pdev);
    }
    return dev;

}
EXPORT_SYMBOL(platform_dev_to_net_dev);

void ol_ath_check_and_enable_btcoex_support(void *bdev, ol_ath_soc_softc_t *soc)
{
	struct platform_device *pdev = (struct platform_device *)bdev;
	struct device_node *dev_node = pdev->dev.of_node;
        int btcoex_support = 0;

	/* Get btcoex HW support info from device tree */
	if (of_property_read_u32(dev_node, "btcoex_support", &btcoex_support)) {
                wlan_psoc_nif_feat_cap_clear(soc->psoc_obj,
					WLAN_SOC_F_BTCOEX_SUPPORT);
	} else {
                if(btcoex_support) {
                     wlan_psoc_nif_feat_cap_set(soc->psoc_obj,
					WLAN_SOC_F_BTCOEX_SUPPORT);
                }
		if (of_property_read_u32(dev_node, "wlan_prio_gpio", &soc->btcoex_gpio)) {
			soc->btcoex_gpio = 0;
                        wlan_psoc_nif_feat_cap_clear(soc->psoc_obj,
					WLAN_SOC_F_BTCOEX_SUPPORT);
		}
	}
	printk("btcoex_support %d wlan_prio_gpio %d\n",btcoex_support, soc->btcoex_gpio);
}

uint32_t ol_ath_get_hw_mode_id(void *bdev)
{
    struct platform_device *pdev = (struct platform_device *)bdev;
    struct device_node *dev_node = pdev->dev.of_node;
    uint32_t hw_mode_id;

    /* Get HW mode id info from device tree */
    if (of_property_read_u32(dev_node, "qcom,hw-mode-id", &hw_mode_id)) {
        qdf_print("No hw_mode_id entry in Device tree. Using hw_mode_id %d",WMI_HOST_HW_MODE_DBS);
        hw_mode_id = WMI_HOST_HW_MODE_DBS;
    }

    return hw_mode_id;
}

bool ol_ath_supported_dev(const struct platform_device_id *id)
{
	switch (id->driver_data) {
		case IPQ4019_DEVICE_ID:
#ifdef QCA_WIFI_QCA8074
		case QCA8074_DEVICE_ID:
#endif
#ifdef QCA_WIFI_QCA8074V2
		case QCA8074V2_DEVICE_ID:
#endif
#ifdef QCA_WIFI_QCA6018
		case QCA6018_DEVICE_ID:
#endif
			return true;
		default:
			return false;
	}
	return false;
}
EXPORT_SYMBOL(ol_ath_supported_dev);

extern int driver_initialized;
int qca_ol_exit_in_progress = 0;
EXPORT_SYMBOL(qca_ol_exit_in_progress);
extern unsigned int testmode;
#ifdef CONFIG_CNSS2_SUPPORT
int ol_ath_ahb_ssr(struct platform_device *pdev,
				struct platform_device_id *id,
				  unsigned long code)
{
    struct ol_ath_soc_softc *soc = NULL;
    int ret = -1;
    int recovery_enable = 0;
    int recovery_in_progress = 0;
    struct net_device *dev;
    struct common_hif_handle *hif_hdl = NULL;
    struct pdev_op_args oper;

    qdf_print("%s: SSR event %d", __FUNCTION__, code);

    if (!pdev)
        return NOTIFY_DONE;

    /*
     * 'dev' and 'soc' are only valid if the driver is already
     * in initialized. The first time they are NULL.
     */
    dev = platform_dev_to_net_dev(pdev);
    if (dev) {
        soc = ath_netdev_priv(dev);
        if (soc) {
            recovery_enable = soc->recovery_enable;
            recovery_in_progress = soc->recovery_in_progress;
            hif_hdl = lmac_get_hif_hdl(soc->psoc_obj);
        } else {
            qdf_print("%s: Invalid soc pointer", __func__);
            return NOTIFY_DONE;
        }
    }

    /* Now handle the notification event from SSR */
    switch (code) {
    case SUBSYS_BEFORE_SHUTDOWN:
        if(testmode == QDF_GLOBAL_COLDBOOT_CALIB_MODE ||
	   testmode == QDF_GLOBAL_WALTEST_MODE) {
            return NOTIFY_DONE;
        }
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (soc && soc->nss_soc.ops) {
            qdf_print("nss-wifi: soc_stop soc->soc_idx: %d ", soc->soc_idx);
            soc->nss_soc.ops->nss_soc_wifi_stop(soc);
        } else if (soc == NULL) {
            qdf_print("%s: Invalid soc pointer", __func__);
            return NOTIFY_DONE;
        }
#endif
        if (qca_ol_exit_in_progress) {
            ol_ath_ahb_remove(pdev);
            driver_initialized = 0;
        } else {
            if (soc->down_complete) {
                /* Target already down */
                return NOTIFY_DONE;
            }

            /* Current Hawkeye FW is common for all Radios. Any FW assert on a SOC,
             * needs update to all SCNs associated with it.
             */

            oper.type = PDEV_ITER_TARGET_ASSERT;
            wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                             wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
            soc->recovery_in_progress = 1;

            soc->target_status = OL_TRGET_STATUS_RESET;
            /* Per Radio tgt_asserts is incremented above, increment for the SOC here */
            soc->soc_stats.tgt_asserts++;

            QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Resetting the radio\n");
            /* Take the rtnl_lock, so that there is no user intervention while
             * we recover.*/
            rtnl_lock();
            ol_ath_ahb_recovery_remove(pdev);
            rtnl_unlock();

        }
        break;
    case SUBSYS_AFTER_SHUTDOWN:
        break;
    case SUBSYS_RAMDUMP_NOTIFICATION:
        /* Send the FWDUMP_READY event to acfg daemon.
         * If hotplug is disabled, then acfg daemon will collect the dump
         */
#if UMAC_SUPPORT_ACFG
        /* Just send the event on 1st scn. The RAMDUMP is common for a soc */
        if (soc) {
            struct wlan_objmgr_pdev *pdev_obj;
            struct ieee80211com *ic;

            pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_SB_ID);
            if (pdev_obj == NULL) {
                qdf_print("%s: pdev object is NULL in RAMDUMP notification",
                                                __func__);
                break;
            }
            ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
            if (ic) {
                OSIF_RADIO_DELIVER_EVENT_WATCHDOG(ic, ACFG_WDT_FWDUMP_READY);
            }
            else {
                qdf_err("ic object is NULL in RAMDUMP notification");
            }

            wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_SB_ID);
        }
#endif
        break;
    case SUBSYS_PREPARE_FOR_FATAL_SHUTDOWN:
        /* Unlike other notification, this event is sent from
         * interrupt context, immediately after the Target Asserts.
         * So do minimal and let the BEFORE_SHUDOWN do the rest
         */

        if (!qca_ol_exit_in_progress) {
            /*
             * This is the Q6FW crash case.
             * set the Targt status to reset and disable interrupts
             * so that no more interrupts are serviced.
             * Also stop tx queue of every VAP.
             */
            if (hif_hdl && soc) {
                if (!recovery_enable){
                    qdf_err("[%s]: XXX TARGET ASSERTED XXX",
                              soc->sc_osdev->netdev->name);
                    /* crash the system */
                    qdf_target_assert_always(0);
                }

                /* Set Target status to RESET */
                hif_set_target_status((struct hif_opaque_softc *)hif_hdl, TARGET_STATUS_RESET);

                /* Stop tx queue of VAPs*/
                ol_ath_tx_stop(soc);

                /* stop target_if timers */
                ol_ath_target_timer_stop(soc);

                /* Disable interrupts */
                hif_nointrs(HIF_GET_SOFTC(hif_hdl));

                /* Handle pdev level operation.
                 * This will set ic level recovery flag
                 * and also calls wmi_stop for each pdev
                 */
                oper.type = PDEV_ITER_FATAL_SHUTDOWN;
                wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                        wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

                /* set soc level flag */
                soc->recovery_in_progress = 1;

            } else {
                qdf_print("Early assert cannot be handled");
                return NOTIFY_DONE;
            }
        }

        break;
    case SUBSYS_BEFORE_POWERUP:
        break;
    case SUBSYS_AFTER_POWERUP:
    {
#if UMAC_SUPPORT_ACFG
        struct wlan_objmgr_pdev *pdev_obj;
        struct ieee80211com *ic;
#endif


        if (recovery_enable && recovery_in_progress) {
            rtnl_lock();
            ret = ol_ath_ahb_recovery_probe(pdev, NULL);
            rtnl_unlock();
            if (ret != 0) {
                qdf_print("Recovery probe failed ");
#if UMAC_SUPPORT_ACFG
                pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_NB_ID);
                if (pdev_obj == NULL) {
                    qdf_print("%s: pdev object is NULL in AFTER POWERUP notification",
                            __func__);
                    break;
                }
                ic = wlan_pdev_get_mlme_ext_obj(pdev_obj);
                if (ic) {
                    OSIF_RADIO_DELIVER_EVENT_WATCHDOG(ic, ACFG_WDT_INIT_FAILED);
                }
                else {
                    qdf_err("ic object is NULL in RAMDUMP notification");
                }
                wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
#endif
                break;
            }
            /* Send event to user space */
            oper.type = PDEV_ITER_POWERUP;
            wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                    wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

            soc->recovery_in_progress = 0;
            soc->target_status = OL_TRGET_STATUS_CONNECTED;

        } else {
            if(!driver_initialized) {
                ol_ath_ahb_probe(pdev, id);
                driver_initialized = 1;
            }
        }
    }
        break;
    default:
	break;
    }

    return NOTIFY_DONE;
}
#endif

int ol_ath_do_waltest(struct platform_device *pdev,
			const struct platform_device_id *id,
			qdf_device_t  qdf_dev)
{
	qdf_info("%s Sending QMI mode as waltest(%d)\n",__func__,
			QDF_GLOBAL_WALTEST_MODE);
	if(pld_wlan_enable(&pdev->dev, NULL, QDF_GLOBAL_WALTEST_MODE,
					"WIN")) {
		qdf_err("%s Error in pld_wlan_enable",__func__);
		goto err;
	}

	return 0;
err:
	qdf_err("Error while doing waltest\n");
	return -EINVAL;
}

#define HIF_IC_MAX_IRQ 52
irqreturn_t hif_dummy_irq_handler(int irq, void *context)
{
	return IRQ_HANDLED;
}
extern const char *ic_irqname[];
int register_free_dummy_irq(struct platform_device *pdev, int isregister)
{
	const char *irq_name;
	int j;
	int irq = 0, ret = 0;
	int irq_entry_count = HIF_IC_MAX_IRQ;
	for (j = 0; j < irq_entry_count; j++) {
		irq_name = ic_irqname[j];
		qal_vbus_get_irq((struct qdf_pfm_hndl *)pdev, irq_name, &irq);
		if (irq < 0) {
			continue;
		}

		if (isregister) {
			ret = request_irq(irq, hif_dummy_irq_handler,
					IRQF_TRIGGER_RISING,
					ic_irqname[j],
					NULL);
		} else {
			free_irq(irq, NULL);
		}
	}
	return 0;
}

extern int register_free_dummy_irq(struct platform_device *pdev, int isregister);
int ol_ath_do_cold_boot_calibration(struct platform_device *pdev,
			const struct platform_device_id *id,
			qdf_device_t  qdf_dev)
{
	int coldboot_wait_limit = 0;
        register_free_dummy_irq(pdev, 1);
	if(pld_wlan_enable(&pdev->dev, NULL, QDF_GLOBAL_COLDBOOT_CALIB_MODE,
					"WIN")) {
		qdf_print("Error in pld_wlan_enable");
		goto err;
	}
	printk("Coldboot calibration wait started\n");
	while(pld_is_cold_boot_cal_done(&pdev->dev) == 0) {
		if (++coldboot_wait_limit >= PLD_COLD_BOOT_MAX_WAIT_LIMIT) {
		    qdf_err("Coldboot Calibration timeout %d secs",
			      coldboot_wait_limit / 10);
		    cnss_dump_qmi_history();
		    qdf_assert_always(0);
		}
		mdelay(PLD_FW_READY_DELAY);
        }
	printk("Coldboot calibration wait ended\n");
	if (pld_wlan_disable(&pdev->dev, CNSS_OFF))
		qdf_print("Error in pld_wlan_disable");
	else
		qdf_print("Coldboot calib successful: radio_id 0x%d",
							id->driver_data);

       register_free_dummy_irq(pdev, 0);
	return 0;
err:
	printk("Error while doing cold boot calibration\n");
	return -EINVAL;
}
int
ol_ath_ahb_probe(struct platform_device *pdev, const struct platform_device_id *id)
{

    struct ol_attach_t ol_cfg;
    void *hif_context = NULL;
    struct hif_target_info *tgt_info = NULL;
    struct _NIC_DEV *aps_osdev = NULL;
    qdf_device_t  qdf_dev = NULL;    /* qdf handle */
    struct pld_plat_data *cnss_plat, *wlan_sc;
    int ret = 0;
    ol_ath_soc_softc_t *soc;
    bool flag = false;
    int count = 0;
    struct pdev_op_args oper;

    if (!ol_ath_supported_dev(id)) {
        qdf_print("Unsupported device");
        ret = -EIO;
        goto err_notsup;
    }

    if ((id->driver_data == QCA8074_DEVICE_ID) ||
        (id->driver_data == QCA8074V2_DEVICE_ID) ||
        (id->driver_data == QCA6018_DEVICE_ID)) {
	    while(pld_is_fw_ready(&pdev->dev) == 0) {
                    mdelay(PLD_FW_READY_DELAY);
		    if (count++ > PLD_FW_READY_TIMEOUT) {
			    qdf_info("Error: QMI FW ready timeout %d seconds",
                                    PLD_FW_READY_TIMEOUT/10);
			    cnss_dump_qmi_history();
			    qdf_assert_always(0);
		    }
 	    }
     }

    aps_osdev = qdf_mem_malloc(sizeof(*aps_osdev));
    if (aps_osdev == NULL) {
        printk("%s %d mem alloc failure \n",__func__,__LINE__);
        ret = -ENOMEM;
        goto err_notsup;
    }
    OS_MEMSET(aps_osdev, 0, sizeof(*(aps_osdev)));
    aps_osdev->bdev = pdev;
    aps_osdev->device = &pdev->dev;
    aps_osdev->bc.bc_bustype = QDF_BUS_TYPE_AHB;

    /* initialize the qdf_dev handle */
    qdf_dev = qdf_mem_malloc(sizeof(*(qdf_dev)));
    if (qdf_dev == NULL) {
        printk("%s %d mem alloc failure \n",__func__,__LINE__);
        ret = -ENOMEM;
        goto cleanup_aps_dev;
    }

    OS_MEMSET(qdf_dev, 0, sizeof(*(qdf_dev)));
    qdf_dev->drv_hdl = NULL; /* bus handle */
    qdf_dev->dev = &pdev->dev; /* device */
    qdf_dev->drv = aps_osdev;

    if (((id->driver_data == QCA8074_DEVICE_ID) ||
         (id->driver_data == QCA8074V2_DEVICE_ID) ||
         (id->driver_data == QCA6018_DEVICE_ID)) &&
		(testmode == QDF_GLOBAL_COLDBOOT_CALIB_MODE)) {
	ret = ol_ath_do_cold_boot_calibration(pdev, id, qdf_dev);
	qdf_mem_free(qdf_dev);
	qdf_mem_free(aps_osdev);
        return ret;
    }

    if (((id->driver_data == QCA8074_DEVICE_ID) ||
         (id->driver_data == QCA8074V2_DEVICE_ID) ||
         (id->driver_data == QCA6018_DEVICE_ID)) &&
		testmode == QDF_GLOBAL_WALTEST_MODE) {
	ret = ol_ath_do_waltest(pdev, id, qdf_dev);
	kfree(qdf_dev);
	kfree(aps_osdev);
	return ret;
    }

    hif_context  = (void *)ol_hif_open(&pdev->dev, (void *)pdev, (void *)id, QDF_BUS_TYPE_AHB, 0,qdf_dev);
    if(hif_context == NULL) {
        printk("Error in ol_hif_open\n");
        ret = -ENOMEM;
        goto cleanup_adf_dev;
    }

    tgt_info = hif_get_target_info_handle(hif_context);
    if(!tgt_info) {
        ret = -EIO;
        goto err;
    }

    ol_cfg.devid = id->driver_data;
    ol_cfg.bus_type = BUS_TYPE_AHB;
    ol_cfg.target_type = tgt_info->target_type;
    ol_cfg.platform_devid = (void *)id;
    ol_cfg.vendor_id = 0x00000000; /* There is no vendorid for AHB devices,
                                      hence assigning 0x00000000 */

    ret = __ol_ath_attach(hif_context, &ol_cfg, aps_osdev, qdf_dev);
    if(ret != 0) {
        ret = -EIO;
        goto cleanup_hif_context;
    }

    soc = ath_netdev_priv(aps_osdev->netdev);

    oper.type = PDEV_ITER_PDEV_ENTRY_ADD;
    oper.ret_val = PDEV_ITER_STATUS_INIT;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                     wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
    if (oper.ret_val == PDEV_ITER_STATUS_OK) {
        flag = true;
    } else if (oper.ret_val == PDEV_ITER_STATUS_FAIL) {
        flag = true;
        goto err_attach;
    }

    ol_ath_diag_user_agent_init(soc);
#ifdef CONFIG_CNSS2_SUPPORT
    if ((id->driver_data == QCA8074_DEVICE_ID) ||
        (id->driver_data == QCA8074V2_DEVICE_ID) ||
        (id->driver_data == QCA6018_DEVICE_ID)) {
	    wlan_sc = qdf_mem_malloc(sizeof(struct pld_plat_data));
	    if(!wlan_sc) {
		    qdf_print("memory allocation fails at cnss_plat_data allocation");
		    goto err_attach;
	    }
	    wlan_sc->wlan_priv = (void *)aps_osdev->netdev;
	    cnss_plat = platform_get_drvdata(pdev);
	    cnss_plat->wlan_priv = wlan_sc;
    } else {
        platform_set_drvdata(pdev, aps_osdev->netdev);
    }
#endif

    return 0;

err_attach:
    if (flag) {
       oper.type = PDEV_ITER_PDEV_ENTRY_DEL;
       wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                         wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
    }


err:
cleanup_hif_context:
    if (hif_context) {
        hif_disable_isr(hif_context);
        ol_hif_close(hif_context);
    }
cleanup_adf_dev:
    qdf_mem_free(qdf_dev);
cleanup_aps_dev:
    qdf_mem_free(aps_osdev);
err_notsup:
    qdf_info("%s failure ret %d",  __func__, ret);
#ifdef QCA_WIFI_QCA8074_VP
    qdf_info("%s SPINNING for host debugging \n",  __func__);
    while(1);
#endif
    qdf_target_assert_always(0);
    return ret;
}
qdf_export_symbol(ol_ath_ahb_probe);

void
ol_ath_ahb_recovery_remove(struct platform_device *pdev)
{
    struct net_device *dev = platform_dev_to_net_dev(pdev);
    ol_ath_soc_softc_t *soc;
    struct pdev_op_args oper;

    soc = ath_netdev_priv(dev);

    if (dev)
        dev_change_flags(dev, dev->flags & (~IFF_UP));

    oper.type = PDEV_ITER_RECOVERY_AHB_REMOVE;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                            wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_VDEV_OP, ath_vdev_recover_delete,
                                            NULL, 1, WLAN_MLME_NB_ID);

    oper.type = PDEV_ITER_RECOVERY_WAIT;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                            wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

    ol_ath_soc_stop(soc, 0);

    oper.type = PDEV_ITER_RECOVERY_STOP;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                          wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
}
qdf_export_symbol(ol_ath_ahb_recovery_remove);

int
ol_ath_ahb_recovery_probe(struct platform_device *pdev, const struct platform_device_id *id)
{
    struct net_device *dev = platform_dev_to_net_dev(pdev);
    struct ol_ath_soc_softc *soc = ath_netdev_priv(dev);
    struct pdev_op_args oper;
    int32_t ret = -1;

    ret = ol_ath_soc_start(soc);
    if(ret != 0) {
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY,
                    QDF_TRACE_LEVEL_INFO, "ERROR: Soc start failed\n");
        return ret;
    }

    oper.type = PDEV_ITER_RECOVERY_PROBE;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                       wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

    return ret;
}
qdf_export_symbol(ol_ath_ahb_recovery_probe);

#define REMOVE_VAP_TIMEOUT_COUNT  20
#define REMOVE_VAP_TIMEOUT        (HZ/10)
void
ol_ath_ahb_remove(struct platform_device *pdev)
{
    struct net_device *dev;
    ol_ath_soc_softc_t *soc;
    u_int32_t target_ver,target_type;
    struct hif_opaque_softc *sc;
#ifdef CONFIG_CNSS2_SUPPORT
    struct pld_plat_data *cnss_plat, *wlan_sc;
#endif
    void __iomem *mem;
    int target_paused = TRUE;
    int waitcnt = 0;
    void *hif_cxt = NULL;
    struct _NIC_DEV *aps_osdev = NULL;
    qdf_device_t  qdf_dev = NULL;    /* qdf handle */
    struct platform_device_id *id;
    wmi_unified_t wmi_handle;
    struct common_hif_handle *hif_hdl;
    struct pdev_op_args oper;

    dev = platform_dev_to_net_dev(pdev);

#ifdef CONFIG_CNSS2_SUPPORT
    id = ath_ahb_get_pdev_id(pdev);
    if (id && ((id->driver_data == QCA8074_DEVICE_ID) ||
               (id->driver_data == QCA8074V2_DEVICE_ID) ||
               (id->driver_data == QCA6018_DEVICE_ID)) &&
		(testmode == QDF_GLOBAL_COLDBOOT_CALIB_MODE)) {
	return;
    }

    if (id && ((id->driver_data == QCA8074_DEVICE_ID) ||
               (id->driver_data == QCA8074V2_DEVICE_ID) ||
               (id->driver_data == QCA6018_DEVICE_ID)) &&
		(testmode == QDF_GLOBAL_WALTEST_MODE)) {
	    if (pld_wlan_disable(&pdev->dev, CNSS_OFF))
		    qdf_err("Error in pld_wlan_disable\n");
	    else
		    qdf_info("QMI mode off sent to target\n");

	    return;
    }
#endif

    /* Attach did not succeed, all resources have been
     * freed in error handler
     */
    if (!dev) {
        qdf_print("dev is null");
        return;
    }
    soc = ath_netdev_priv(dev);
    hif_hdl = lmac_get_hif_hdl(soc->psoc_obj);
    hif_cxt = hif_hdl;
    sc = (struct hif_opaque_softc *)hif_hdl;

    mem = (void __iomem *)dev->mem_start;

    __ol_vap_delete_on_rmmod(dev);

    while (atomic_read(&soc->reset_in_progress)  && (waitcnt < REMOVE_VAP_TIMEOUT_COUNT)) {
        schedule_timeout_interruptible(REMOVE_VAP_TIMEOUT);
        waitcnt++;
    }

    /* Suspend the target if it is not done during the last vap removal */
    if (!(soc->down_complete)) {
        /* Suspend Target */
        qdf_print("Suspending Target - with disable_intr set :%s (sc %pK) soc=%pK",dev->name,sc, soc);
        if (!ol_ath_suspend_target(soc, 1)) {
            u_int32_t  timeleft;
            qdf_print("waiting for target paused event from target :%s (sc %pK)",dev->name,sc);
            /* wait for the event from Target*/
            timeleft = wait_event_interruptible_timeout(soc->sc_osdev->event_queue,
                    (soc->is_target_paused == TRUE),
                    200);
            if(!timeleft || signal_pending(current)) {
                qdf_print("ERROR: Failed to receive target paused event :%s (sc %pK)",dev->name,sc);
                target_paused = FALSE;
            }
            /*
             * reset is_target_paused and host can check that in next time,
             * or it will always be TRUE and host just skip the waiting
             * condition, it causes target assert due to host already suspend
             */
            soc->is_target_paused = FALSE;
        }
    }
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    qdf_print("AHB Call to nss detach ");
    if (!(soc->down_complete)) {

            qdf_print("Call to nss detach ops %pK ", soc->nss_soc.ops);
            if (soc->nss_soc.ops) {
            qdf_print("nss-wifi: detach ");
            soc->nss_soc.ops->nss_soc_wifi_detach(soc, 100);
        }
    }
#endif

    /* Copy the pointer as netdev will be freed in __ol_ath_detach */
    aps_osdev = soc->sc_osdev;
    qdf_dev = soc->qdf_dev;

    if (!soc->down_complete) {
        ol_ath_diag_user_agent_fini(soc);
    }
    /* save target_version since soc is not valid after __ol_ath_detach */
    target_ver = lmac_get_tgt_version(soc->psoc_obj);
    target_type = lmac_get_tgt_type(soc->psoc_obj);
    wmi_handle = lmac_get_wmi_unified_hdl(soc->psoc_obj);


    oper.type = PDEV_ITER_PDEV_ENTRY_DEL;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                      wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);

    if (target_paused == TRUE) {
        __ol_ath_detach(dev);
    } else {
        soc->fwsuspendfailed = 1;
        wmi_stop(wmi_handle);
    }

    if (target_paused != TRUE) {
        __ol_ath_detach(dev);
    }

    if(aps_osdev)
        qdf_mem_free(aps_osdev);
    if(qdf_dev)
        qdf_mem_free(qdf_dev);

#ifdef CONFIG_CNSS2_SUPPORT
    if (id && ((id->driver_data == QCA8074_DEVICE_ID) ||
               (id->driver_data == QCA8074V2_DEVICE_ID) ||
               (id->driver_data == QCA6018_DEVICE_ID))) {
	    cnss_plat = platform_get_drvdata(pdev);
	    wlan_sc  = cnss_plat->wlan_priv;
	    cnss_plat->wlan_priv = NULL;
	    wlan_sc->wlan_priv = NULL;
	    qdf_mem_free(wlan_sc);
    }
#endif
    printk(KERN_INFO "ath_ahb_remove\n");
}
qdf_export_symbol(ol_ath_ahb_remove);

#if OL_ATH_SUPPORT_LED
/* Temporarily defining the core_sw_output address here as
ar900b,qca9884,qca9888 didn't define the macro in FW cmn headers */
#define CORE_SW_OUTPUT 0x82004

void ol_ath_led_event(struct ol_ath_softc_net80211 *scn, OL_LED_EVENT event);
extern bool ipq4019_led_initialized;
extern uint32_t ipq4019_led_type;

void ipq4019_wifi_led(struct ol_ath_softc_net80211 *scn, int on_or_off)
{
    struct hif_opaque_softc *hif_hdl;
    hif_hdl = lmac_get_ol_hif_hdl(scn->soc->psoc_obj);

    if (!hif_hdl)
        return;

    if (!ipq4019_led_initialized) {
        return;
    }
    if ((ipq4019_led_type == IPQ4019_LED_GPIO_PIN) &&
        (scn->scn_led_gpio > 0)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
            gpio_set_value_cansleep(scn->scn_led_gpio, on_or_off);
#endif
    } else if (ipq4019_led_type == IPQ4019_LED_SOURCE) {
        hif_reg_write(hif_hdl, CORE_SW_OUTPUT, on_or_off);
    }
    return;
}

uint8_t ipq4019_is_wifi_gpio_shared(struct ol_ath_softc_net80211 *scn)
{
    ol_ath_soc_softc_t *tmp_soc = NULL;
    ol_ath_soc_softc_t *curr_soc = scn->soc;
    int32_t soc_idx = 0;
    int32_t soc_num = 0;
    struct pdev_op_args oper;

    /* check within current soc first */
    oper.type = PDEV_ITER_LED_GPIO_STATUS;
    oper.pointer = (void *)scn;
    oper.ret_val = PDEV_ITER_STATUS_INIT;

    wlan_objmgr_iterate_obj_list(curr_soc->psoc_obj, WLAN_PDEV_OP,
              wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
    if (oper.ret_val == PDEV_ITER_STATUS_OK)
        return TRUE;

    /* check within other socs */
    while ((soc_num < ol_num_global_soc) && (soc_idx < GLOBAL_SOC_SIZE)) {
        tmp_soc = ol_global_soc[soc_idx++];
        if (tmp_soc == NULL)
            continue;
        soc_num++;
        if (tmp_soc == curr_soc)
            continue;
        else {
            oper.type = PDEV_ITER_LED_GPIO_STATUS;
            oper.pointer = (void *)scn;
            oper.ret_val = PDEV_ITER_STATUS_INIT;
            wlan_objmgr_iterate_obj_list(tmp_soc->psoc_obj, WLAN_PDEV_OP,
                      wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
            if (oper.ret_val == PDEV_ITER_STATUS_OK)
                  return TRUE;
        }
    } /* while */

    return FALSE;
}

void ipq4019_wifi_led_init(struct ol_ath_softc_net80211 *scn)
{
    struct platform_device *pdev = (struct platform_device *)(scn->sc_osdev->bdev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
    uint32_t wifi_core_id=0xffffffff;
    int32_t led_gpio = 0;
    uint32_t led_num = 0;
    uint32_t led_source = 0;
    int32_t ret = -1;
#endif // linux kernel > 3.14

    if (!pdev ) {
        return;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
    if (of_property_read_u32(pdev->dev.of_node, "core-id", &wifi_core_id) != 0) {
        printk("%s: Wifi LED init failed.. Couldn't get core id\r\n", __func__);
        return;
    }

    led_gpio = of_get_named_gpio(pdev->dev.of_node, "wifi-led-gpios", 0);
    if (led_gpio <= 0) {
        /* no led gpio.. get led source */
        if ((ret = of_property_read_u32(pdev->dev.of_node, "wifi_led_num", &led_num)) ||
            (ret = of_property_read_u32(pdev->dev.of_node, "wifi_led_source", &led_source))) {
            printk("%s: Wifi LED init failed.. Couldn't get led gpio/led source\r\n", __func__);
            return;
        }
    }

    if ((wifi_core_id == 0) || (wifi_core_id == 1)) {
        if ((led_gpio > 0) && (gpio_is_valid(led_gpio))) {
            int32_t ret_status = 0;
            ipq4019_led_type = IPQ4019_LED_GPIO_PIN; // led type gpio
            scn->scn_led_gpio = led_gpio;
            /* check if this gpio is already requested/shared between wifi radios */
            if (!ipq4019_is_wifi_gpio_shared(scn)) {
                //ret_status = gpio_request_one(led_gpio, GPIOF_OUT_INIT_LOW, "wifi-led-gpios");
                ret_status = gpio_request_one(led_gpio, GPIOF_DIR_OUT, "wifi-led-gpios");
                if (ret_status != 0) {
                    printk("%s: Wifi LED gpio request failed.. \r\n", __func__);
                    return;
                }
            }
        } else if (led_num > 0) {
            ipq4019_led_type = IPQ4019_LED_SOURCE; // led source selected
#ifdef CONFIG_LEDS_IPQ
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0))
	    ipq_led_source_select(led_num, led_source);
#else
            ipq40xx_led_source_select(led_num, led_source);
#endif
#endif
            scn->scn_led_gpio = 0;
        }
    }

    if ((led_gpio > 0) || (led_num > 0)) {
        ipq4019_led_initialized = 1;
    }

#endif /* linux kernel > 3.14 */
    return;
}

void ipq4019_wifi_led_deinit(struct ol_ath_softc_net80211 *scn)
{
    if (ipq4019_led_type == IPQ4019_LED_GPIO_PIN) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0)
        if ((scn->scn_led_gpio) && (gpio_is_valid(scn->scn_led_gpio))) {
            /* check if this gpio is freed already by another scn context */
            if (!ipq4019_is_wifi_gpio_shared(scn)) {
                gpio_set_value_cansleep(scn->scn_led_gpio, 0);
                    if (qal_vbus_release_iorsc(scn->scn_led_gpio)) {
                    qdf_warn("Failed to free led gpio no: %d",
                              scn->scn_led_gpio);
                }
            }
        }
#endif
    } else if (ipq4019_led_type == IPQ4019_LED_SOURCE) {
        /* nothing to be done */
    }
    scn->scn_led_gpio = 0;
    ipq4019_led_initialized = 0;
}

struct valid_reg_range {
    uint32_t start;
    uint32_t end;
} ipq4019_soc_reg_range[] = {
    { 0x080000, 0x080000 },
    { 0x080020, 0x080020 },
    { 0x080028, 0x080050 },
    { 0x0800d4, 0x0800ec },
    { 0x08010c, 0x080118 },
    { 0x080284, 0x080290 },
    { 0x0802a8, 0x0802b8 },
    { 0x0802dc, 0x08030c },
    { 0x082000, 0x083fff }};

QDF_STATUS hif_diag_read_soc_ipq4019(struct hif_opaque_softc *hif_device, uint32_t address, uint8_t *data, int nbytes)
{
    uint32_t *ptr= (uint32_t *)data;
    uint32_t pSaddr,pEaddr,pRSaddr,pREaddr;
    int range = 0;

    if( nbytes % 4 ){
        printk("Pls check length value:%s,length:%x\n",__func__,nbytes);
    }

    for(range = 0; range < (nbytes/4); range++) {
        ptr[range] = 0xdeadbeef;
    }

    pSaddr = address;
    pEaddr = address + nbytes - 4;

    for(range = 0; range < 9; range++){

        if( ipq4019_soc_reg_range[range].start >= pSaddr &&  ipq4019_soc_reg_range[range].start <= pEaddr ){

            pRSaddr = ipq4019_soc_reg_range[range].start;

            if(ipq4019_soc_reg_range[range].end <= pEaddr){
                pREaddr = ipq4019_soc_reg_range[range].end;
            }else{
                pREaddr = pEaddr;
            }

        }else if( pSaddr > ipq4019_soc_reg_range[range].start  && pSaddr <= ipq4019_soc_reg_range[range].end ){

            pRSaddr = pSaddr;
            if(ipq4019_soc_reg_range[range].end <= pEaddr){
                pREaddr = ipq4019_soc_reg_range[range].end;
            }else{
                pREaddr = pEaddr;
            }

        }else {
            continue;
        }

        hif_diag_read_mem(hif_device,pRSaddr,data+(pRSaddr-address),pREaddr-pRSaddr+4);
    }
    return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(hif_diag_read_soc_ipq4019);
#endif
