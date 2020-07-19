/*
 * Copyright (c) 2013,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <osdep.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/if_arp.h>
#include "ol_if_athvar.h"
#include "ol_ath.h"
#include "hif.h"
#include "hif_napi.h"
#include "ah_devid.h"
#include "pld_common.h"
#include <osapi_linux.h>
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#endif
#include <osif_private.h>
#include <ath_pci.h>
#include "pld_common.h"
#include <init_deinit_lmac.h>

extern struct g_wifi_info g_winfo;

void *ol_hif_open(struct device *dev, void *bdev, void *bid,
        enum qdf_bus_type bus_type, bool reinit, qdf_device_t qdf_dev);
int osif_recover_vap_delete(struct ieee80211com *ic);
void osif_recover_vap_create(struct ieee80211com *ic);
int ol_ath_soc_stop(struct ol_ath_soc_softc *soc, bool flush_wq);
int ol_ath_target_start(struct ol_ath_soc_softc *scn);
void ol_ath_target_timer_stop(struct ol_ath_soc_softc *soc);
void ath_vdev_recover_delete(struct wlan_objmgr_psoc *psoc, void *obj, void *args);
void ath_vdev_dump(struct wlan_objmgr_psoc *psoc, void *obj, void *args);
int osif_recover_profile_alloc(struct ieee80211com *ic);

static int qca_napi_create (void *hif_ctx);
static int qca_napi_destroy (void *hif_ctx, int force);
#if !defined(REMOVE_PKT_LOG) && defined(QCA_LOWMEM_PLATFORM)
extern unsigned int enable_pktlog_support;
#endif

#ifdef CONFIG_CNSS2_SUPPORT
extern struct pld_plat_data *wlan_sc, *cnss_plat;
#endif
typedef struct pld_plat_data pci_priv_data;

extern unsigned int testmode;
extern unsigned int polledmode;
extern void cnss_dump_qmi_history(void);

struct net_device *pci_dev_to_net_dev(struct pci_dev *pdev)
{
    struct net_device *dev = NULL;
    pci_priv_data *p;
#ifdef CONFIG_CNSS2_SUPPORT
    if (pdev->device == QCA6290_DEVICE_ID) {
            p = pci_get_drvdata(pdev);
	    cnss_plat = (struct pld_plat_data *)p->wlan_priv;
	    wlan_sc  = cnss_plat->wlan_priv;
            if(wlan_sc) {
	            dev = (struct net_device *) wlan_sc->wlan_priv;
		    qdf_mem_free(wlan_sc);
	    }
    } else {
            dev = pci_get_drvdata(pdev);
    }
#endif
    return dev;

}

#define BAR_NUM                    0
#ifndef BUILD_X86
int qcom_pcie_register_event(struct qcom_pcie_register_event *reg);

void ol_ath_pci_event(struct qcom_pcie_notify *notify)
{
    struct ol_ath_soc_softc *soc;
#if UMAC_SUPPORT_ACFG
    struct pdev_op_args oper;
#endif
    enum qcom_pcie_event event = notify->event;
    soc = (struct ol_ath_soc_softc*) notify->data;

    if (soc == NULL)
        return;

    switch (event) {
    case QCOM_PCIE_EVENT_LINKDOWN:
        soc->target_status = OL_TRGET_STATUS_RESET;

#if UMAC_SUPPORT_ACFG
        oper.type = PDEV_ITER_PCIE_ASSERT;
        wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                   wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
#endif
        break;

    case QCOM_PCIE_EVENT_LINKUP:
        break;

    default:
        break;
    }
    return;
}
#endif

int
ol_ath_pci_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{

    struct ol_attach_t ol_cfg;
    void *hif_context;
    struct hif_target_info *tgt_info;
    struct _NIC_DEV *aps_osdev = NULL;
    int ret = 0, count = 0;
    qdf_device_t         qdf_dev = NULL;    /* qdf handle */
    ol_ath_soc_softc_t *soc;
    bool flag = false, skip_assert=false;
    pci_priv_data *p = NULL;
    struct pdev_op_args oper;

    aps_osdev = qdf_mem_malloc(sizeof(*aps_osdev));
    if(aps_osdev == NULL) {
	printk("Error while allocating aps_osdev\n");
	ret =-1;
	goto cleanup1;
    }
    OS_MEMSET(aps_osdev, 0, sizeof(*(aps_osdev)));

    aps_osdev->bdev = pdev;
    aps_osdev->device = &pdev->dev;
    aps_osdev->bc.bc_bustype = QDF_BUS_TYPE_PCI;

    /* initialize the qdf_dev handle */
    qdf_dev = qdf_mem_malloc(sizeof(*(qdf_dev)));
    if (qdf_dev == NULL) {
        printk("%s %d mem alloc failure \n",__func__,__LINE__);
        return -1;
    }

    OS_MEMSET(qdf_dev, 0, sizeof(*(qdf_dev)));
    qdf_dev->drv_hdl = pdev; /* bus handle */
    qdf_dev->dev = &pdev->dev; /* device */
    qdf_dev->drv = aps_osdev;
#ifdef CONFIG_CNSS2_SUPPORT
    if(id->device == QCA6290_DEVICE_ID) {
           while(pld_is_fw_ready(&pdev->dev) == 0) {
                    mdelay(PLD_FW_READY_DELAY);
                    if(count++ > PLD_FW_READY_TIMEOUT) {
                            qdf_info("Error: QMI FW ready timeout %d secs",
                                    PLD_FW_READY_TIMEOUT/10);
                            cnss_dump_qmi_history();
                            qdf_assert_always(0);
		    }
            }
    }
#endif //CONFIG_CNSS2_SUPPORT

#ifndef CONFIG_CNSS2_SUPPORT
    pci_set_drvdata(pdev, NULL);
#endif
    hif_context  = ol_hif_open(&pdev->dev, pdev, (void *)id, QDF_BUS_TYPE_PCI, 0,qdf_dev);

    if(hif_context == NULL) {
	ret = -1;
	goto cleanup1;
    }

    tgt_info = hif_get_target_info_handle(hif_context);

    ol_cfg.devid = id->device;
    ol_cfg.bus_type = BUS_TYPE_PCIE;
    ol_cfg.target_type = tgt_info->target_type;
    ol_cfg.target_revision = tgt_info->target_revision;
    ol_cfg.platform_devid = (void *)id;
    ol_cfg.vendor_id = id->vendor;

    ret = __ol_ath_attach(hif_context, &ol_cfg, aps_osdev, qdf_dev);
    if(ret != 0) {
        if(ret == -ENOENT)
            skip_assert = true;
        ret = -EIO;
        goto cleanup2;
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
    if (id->device == QCA6290_DEVICE_ID) {
	    wlan_sc = qdf_mem_malloc(sizeof(struct pld_plat_data));
	    if(!wlan_sc) {
		    qdf_print("memory allocation fails at cnss_plat_data allocation");
		    goto err_attach;
	    }
	    wlan_sc->wlan_priv = (void *)aps_osdev->netdev;
	    p = pci_get_drvdata(pdev);
	    cnss_plat = (struct pld_plat_data *) p->wlan_priv;
	    cnss_plat->wlan_priv = wlan_sc;
    } else {
    	pci_set_drvdata(pdev, aps_osdev->netdev);
    }
#endif
#ifndef BUILD_X86
    /* Register with PCI subsystem for events */
        soc->pcie_notify.data = (void*)soc;
        soc->pcie_event.events = QCOM_PCIE_EVENT_LINKDOWN | QCOM_PCIE_EVENT_LINKUP;
        soc->pcie_event.user = pdev;
        soc->pcie_event.callback = ol_ath_pci_event;
        soc->pcie_event.notify = soc->pcie_notify;
        qcom_pcie_register_event(&soc->pcie_event);
#endif
    return 0;

err_attach:
    if (flag) {
       oper.type = PDEV_ITER_PDEV_ENTRY_DEL;
       wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                      wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
    }
    ret = -EIO;
cleanup2:
    if (hif_context) {
        hif_disable_isr(hif_context);
        ol_hif_close(hif_context);
    }

cleanup1:
    if (qdf_dev) {
	OS_FREE(qdf_dev);
    }
    if (aps_osdev) {
	OS_FREE(aps_osdev);
    }
    if(skip_assert)
        qdf_print("%s: Exiting as FW download failed", __func__);
    else {
        qdf_print("%s failure ret %d",  __func__, ret);
        qdf_target_assert_always(0);
    }
    return ret;
}
qdf_export_symbol(ol_ath_pci_probe);


void pci_reconnect_cb(ol_ath_soc_softc_t *soc)
{
    qdf_sched_work(0, &soc->pci_reconnect_work);
}


/**
 * qca_napi_destroy() - destroys the NAPI structures for a given netdev
 * @force: if set, will force-disable the instance before _del'ing
     *
 * Destroy NAPI instances. This function is called
 * unconditionally during module removal. It destroy
 * napi structures through the proper HTC/HIF calls.
 *
 * Return:
 *    number of NAPI instances destroyed
 */


#ifdef FEATURE_NAPI
static int qca_napi_destroy (void *hif_ctx, int force)
{
    int rc = 0;
    int i;

    NAPI_DEBUG("--> (force=%d)\n", force);
    for (i = 0; i < CE_COUNT_MAX; i++) {
        if (hif_napi_created(hif_ctx, i)) {
            if (0 <= hif_napi_destroy(
                        hif_ctx,
                        NAPI_PIPE2ID(i), force)) {
                rc++;
            } else
                qdf_print("cannot destroy napi %d: (pipe:%d), f=%d",
                        i,
                        NAPI_PIPE2ID(i), force);
        }
    }

    NAPI_DEBUG("<-- [rc=%d]\n", rc);
    return rc;
}

/**
 * qca_napi_poll() - NAPI poll function
 * @napi  : pointer to NAPI struct
 * @budget: the pre-declared budget
 *
 * Implementation of poll function. This function is called
 * by kernel during softirq processing.
 *
 * NOTE FOR THE MAINTAINER:
 *   Make sure this is very close to the ce_tasklet code.
 *
 * Return:
 *   int: the amount of work done ( <= budget )
 */
int qca_napi_poll(struct napi_struct *napi, int budget)
{
	struct qca_napi_info *napi_info;
	napi_info = (struct qca_napi_info *)
		container_of(napi, struct qca_napi_info, napi);
	return hif_napi_poll(napi_info->hif_ctx, napi, budget);
}

static int qca_napi_create (void *hif_ctx)
{
    uint8_t ul, dl;
    int     ul_polled, dl_polled;
    int     rc = 0;

    NAPI_DEBUG("-->\n");
    /*
     * hif_map_service_to_pipe should be changed to consider
     * target type to find service to pipe mapping table and
     * and should serch this table to find correct mapping as
     * Win and MCL may have a different mappings.
     * Right now using default MCL mapping table.
     */
    if (QDF_STATUS_SUCCESS !=
            hif_map_service_to_pipe(hif_ctx, HTT_DATA_MSG_SVC,
                        &dl, &ul, &ul_polled, &dl_polled)) {
            qdf_print("%s: cannot map service to pipe", __func__);
            rc = -EINVAL;
    } else {
        rc = hif_napi_create(hif_ctx, qca_napi_poll, QCA_NAPI_BUDGET, QCA_NAPI_DEF_SCALE, 0);
        if (rc < 0) {
            qdf_print("%s: ERR(%d) creating NAPI on pipe %d", __func__, rc, dl);
        } else {
            qdf_print("%s: napi instance %d created on pipe %d", __func__, rc, dl);
    }
    }
    NAPI_DEBUG("<-- [rc=%d]\n", rc);

    return rc;
}
#endif
void *ol_hif_open(struct device *dev, void *bdev, void *bid,
        enum qdf_bus_type bus_type, bool reinit, qdf_device_t qdf_dev)
{
    QDF_STATUS status;
    int ret = 0;
    void *hif_ctx;
    struct hif_driver_state_callbacks cbk;

        OS_MEMSET(&cbk, 0, sizeof(cbk));
        cbk.context = dev;

        if (testmode == 1) {
          hif_ctx = hif_open(qdf_dev, QDF_GLOBAL_FTM_MODE, bus_type, &cbk);
        } else if (testmode == QDF_GLOBAL_COLDBOOT_CALIB_MODE) {
          hif_ctx = hif_open(qdf_dev, QDF_GLOBAL_COLDBOOT_CALIB_MODE, bus_type,
						&cbk);
        } else {
          hif_ctx = hif_open(qdf_dev, QDF_GLOBAL_MISSION_MODE, bus_type, &cbk);
        }

        if (!hif_ctx) {
            printk("%s: hif_open error", __func__);
            return NULL;
        }
        hif_set_ce_service_max_yield_time(hif_ctx,
                        CE_PER_ENGINE_SERVICE_MAX_YIELD_TIME);
#ifdef QCA_LOWMEM_PLATFORM
        if(!enable_pktlog_support) {
           /*Disable Copy Engine 8 recv buffer enqueue*/
           hif_set_attribute(hif_ctx,HIF_LOWDESC_CE_NO_PKTLOG_CFG);
        } else {
           hif_set_attribute(hif_ctx,HIF_LOWDESC_CE_CFG);
        }
#endif
	status = hif_enable(hif_ctx, dev, bdev, bid, bus_type,
			(reinit == true) ?  HIF_ENABLE_TYPE_REINIT :
			HIF_ENABLE_TYPE_PROBE);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		printk("hif_enable error = %d, reinit = %d",
				status, reinit);
		ret = /*cdf_status_to_os_return(status)*/ -status;
		goto err_hif_close;
	}
#ifdef FEATURE_NAPI
#define NAPI_ANY -1
	ret = qca_napi_create(hif_ctx);
	if (ret < 0) {
		printk("%s: NAPI creation error, rc: 0x%x", __func__, status);
		return NULL;
	}
	hif_napi_event(hif_ctx, NAPI_EVT_CMD_STATE, (void *)0x1);
	if (hif_napi_enabled(hif_ctx, NAPI_ANY)) {
		NAPI_DEBUG("hdd_napi_create returned: %d", status);
		if (ret < 0) {
			qdf_print("NAPI creation error, rc: 0x%x, reinit = %d",
					status, reinit);
			ret = -EFAULT;
			goto err_napi_destroy;
		}
	}
#endif
    return hif_ctx;

#ifdef FEATURE_NAPI
err_napi_destroy:
	qca_napi_destroy(hif_ctx, true);
#endif

err_hif_close:
	hif_close(hif_ctx);
	return NULL;
}

void ol_hif_close(void *hif_ctx)
{
    if (hif_ctx == NULL)
	return;

    hif_disable(hif_ctx, HIF_DISABLE_TYPE_REMOVE);

#ifdef FEATURE_NAPI
    qca_napi_destroy(hif_ctx, true);
#endif
    hif_close(hif_ctx);
}

#define REMOVE_VAP_TIMEOUT_COUNT  20
#define REMOVE_VAP_TIMEOUT        (HZ/10)

void
ol_ath_pci_remove(struct pci_dev *pdev)
{
    struct net_device *dev = pci_dev_to_net_dev(pdev);
    ol_ath_soc_softc_t *soc;
    u_int32_t target_ver;
    struct hif_opaque_softc *sc;
    int target_paused = TRUE;
    int waitcnt = 0;
    struct _NIC_DEV *osdev = NULL;
    qdf_device_t  qdf_dev = NULL;    /* qdf handle */
    wmi_unified_t wmi_handle;
    struct pdev_op_args oper;

    /* Attach did not succeed, all resources have been
     * freed in error handler
     */
    if (!dev)
        return;

    soc = ath_netdev_priv(dev);
    sc = lmac_get_ol_hif_hdl(soc->psoc_obj);

    __ol_vap_delete_on_rmmod(dev);

    while (atomic_read(&soc->reset_in_progress)  && (waitcnt < REMOVE_VAP_TIMEOUT_COUNT)) {
        schedule_timeout_interruptible(REMOVE_VAP_TIMEOUT);
        waitcnt++;
    }

    /* Suspend the target if it is not done during the last vap removal */
    if (!(soc->down_complete)) {

        /* Suspend Target */
        qdf_print(KERN_INFO"Suspending Target - with disable_intr set :%s (sc %pK)",dev->name,sc);
        if (!ol_ath_suspend_target(soc, 1)) {
            u_int32_t  timeleft;
            qdf_print(KERN_INFO"waiting for target paused event from target :%s (sc %pK)",dev->name,sc);
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
    if (!(soc->down_complete)) {
        if (soc->nss_soc.ops) {
            qdf_print("nss-wifi: detach ");
            soc->nss_soc.ops->nss_soc_wifi_detach(soc, 100);
        }
    }
#endif

    if (!soc->down_complete) {
        ol_ath_diag_user_agent_fini(soc);
    }

    /* save target_version since scn is not valid after __ol_ath_detach */

    target_ver = lmac_get_tgt_version(soc->psoc_obj);
    osdev = soc->sc_osdev;
    qdf_dev = soc->qdf_dev;
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

    if(osdev)
        qdf_mem_free(osdev);

    if(qdf_dev)
        qdf_mem_free(qdf_dev);

    if(pdev->device != QCA6290_DEVICE_ID)
	pci_set_drvdata(pdev, NULL);
    printk(KERN_INFO "ath_pci_remove\n");
}
qdf_export_symbol(ol_ath_pci_remove);

#define OL_ATH_PCI_PM_CONTROL 0x44

int
ol_ath_pci_suspend(struct pci_dev *pdev, pm_message_t state)
{
    struct net_device *dev = pci_get_drvdata(pdev);
    u32 val;

    if (__ol_ath_suspend(dev))
        return (-1);

    pci_read_config_dword(pdev, OL_ATH_PCI_PM_CONTROL, &val);
    if ((val & 0x000000ff) != 0x3) {
        PCI_SAVE_STATE(pdev,
            ((struct ath_pci_softc *)ath_netdev_priv(dev))->aps_pmstate);
        pci_disable_device(pdev);
        pci_write_config_dword(pdev, OL_ATH_PCI_PM_CONTROL, (val & 0xffffff00) | 0x03);
    }
    return 0;
}
qdf_export_symbol(ol_ath_pci_suspend);

int
ol_ath_pci_resume(struct pci_dev *pdev)
{
    struct net_device *dev = pci_get_drvdata(pdev);
    u32 val;
    int err;

    err = pci_enable_device(pdev);
    if (err)
        return err;

    pci_read_config_dword(pdev, OL_ATH_PCI_PM_CONTROL, &val);
    if ((val & 0x000000ff) != 0) {
        PCI_RESTORE_STATE(pdev,
            ((struct ath_pci_softc *)ath_netdev_priv(dev))->aps_pmstate);

        pci_write_config_dword(pdev, OL_ATH_PCI_PM_CONTROL, val & 0xffffff00);

        /*
         * Suspend/Resume resets the PCI configuration space, so we have to
         * re-disable the RETRY_TIMEOUT register (0x41) to keep
         * PCI Tx retries from interfering with C3 CPU state
         *
         */
        pci_read_config_dword(pdev, 0x40, &val);

        if ((val & 0x0000ff00) != 0)
            pci_write_config_dword(pdev, 0x40, val & 0xffff00ff);
    }

    if (__ol_ath_resume(dev))
        return (-1);

    return 0;
}
qdf_export_symbol(ol_ath_pci_resume);

void
ol_ath_pci_recovery_remove(struct pci_dev *pdev)
{
    struct net_device *dev = pci_get_drvdata(pdev);
    struct ol_ath_soc_softc *soc;
    struct pdev_op_args oper;

    soc = ath_netdev_priv(dev);
    /* stop target_if timers */
    ol_ath_target_timer_stop(soc);

    oper.type = PDEV_ITER_RECOVERY_REMOVE;
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
qdf_export_symbol(ol_ath_pci_recovery_remove);

int
ol_ath_pci_recovery_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
    struct net_device *dev = pci_get_drvdata(pdev);
    struct ol_ath_soc_softc *soc = ath_netdev_priv(dev);
    struct pdev_op_args oper;
    int ret = -1;

    ret = ol_ath_target_start(soc);
    if(ret != 0) {
        qdf_print("Error: Target start failed");
        return ret;
    }

    oper.type = PDEV_ITER_RECOVERY_PROBE;
    wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                       wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);


    return ret;
}
qdf_export_symbol(ol_ath_pci_recovery_probe);
