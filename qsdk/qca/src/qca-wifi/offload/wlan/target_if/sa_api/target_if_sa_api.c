/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlan_tgt_def_config.h>
#include <target_type.h>
#include <hif_hw_version.h>
#include <ol_if_athvar.h>
#include <target_if_sa_api.h>
#include <target_if.h>
#include <wlan_lmac_if_def.h>
#include <wlan_osif_priv.h>
#include <wlan_mlme_dispatcher.h>
#include <init_deinit_lmac.h>

#define A_IO_READ32(addr)         ioread32((void __iomem *)addr)
#define A_IO_WRITE32(addr, value) iowrite32((u32)(value), (void __iomem *)(addr))


static int
target_if_smart_ant_dummy_assoc_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
    return 0;
}

static int
target_if_smart_ant_assoc_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
	ol_ath_soc_softc_t *scn = (ol_ath_soc_softc_t *) sc;
	uint8_t peer_macaddr[IEEE80211_ADDR_LEN];
	wmi_sa_rate_cap rate_cap;
	struct wlan_objmgr_peer *peer_obj;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	void *tgt_if_handle = 0;
	QDF_STATUS status;

	psoc = scn->psoc_obj;
	if (NULL == psoc) {
		sa_api_err("psoc is NULL\n");
		return -EINVAL;
	}

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_SA_API_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_print("%s, %d unable to get psoc reference", __func__, __LINE__);
		return -EINVAL;
	}


	tgt_if_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == tgt_if_handle) {
		sa_api_err("tgt_if_handle is NULL\n");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_SA_API_ID);
		return -EINVAL;
	}


	if (wmi_extract_peer_ratecode_list_ev(tgt_if_handle,
				data, peer_macaddr, &rate_cap) < 0) {
		qdf_print("Unable to extract peer_ratecode_list_ev ");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_SA_API_ID);
		return -1;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0, WLAN_SA_API_ID);
	if (NULL == pdev) {
		sa_api_err("pdev is %pK\n", pdev);
		wlan_objmgr_psoc_release_ref(psoc, WLAN_SA_API_ID);
		return -EINVAL;
	}

	peer_obj = wlan_objmgr_get_peer(psoc, wlan_objmgr_pdev_get_pdev_id(pdev), peer_macaddr, WLAN_MLME_SB_ID);
	if (peer_obj == NULL) {
		qdf_print("%s: Unable to find peer object", __func__);
		wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
		wlan_objmgr_psoc_release_ref(psoc, WLAN_SA_API_ID);
		return -1;
	}

	/* peer connect */
	target_if_sa_api_peer_assoc_hanldler(psoc, pdev, peer_obj, (struct sa_rate_cap *)&rate_cap);

	wlan_objmgr_peer_release_ref(peer_obj, WLAN_MLME_SB_ID);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
	wlan_objmgr_psoc_release_ref(psoc, WLAN_SA_API_ID);

	return 0;
}

void target_if_sa_api_register_wmi_event_handler(struct wlan_objmgr_psoc *psoc)
{
	if (NULL == psoc) {
		sa_api_err("PSOC is NULL!\n");
		return;
	}

	if (target_if_sa_api_get_sa_enable(psoc)) {
	    wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_peer_ratecode_list_event_id,
			target_if_smart_ant_assoc_handler,
			WMI_RX_UMAC_CTX);
	} else {
	    wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_peer_ratecode_list_event_id,
			target_if_smart_ant_dummy_assoc_handler,
			WMI_RX_UMAC_CTX);

	}
}

void target_if_sa_api_unregister_wmi_event_handler(struct wlan_objmgr_psoc *psoc)
{
	if (NULL == psoc) {
		sa_api_err("PSOC is NULL!\n");
		return;
	}

	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_peer_ratecode_list_event_id);
}


void target_if_sa_api_set_tx_antenna (struct wlan_objmgr_peer *peer, uint32_t *antenna_array)
{
	struct smart_ant_tx_ant_params param;
	uint8_t *mac = NULL;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;

	qdf_mem_set(&param, sizeof(param), 0);

	param.antenna_array = antenna_array;

	mac = wlan_peer_get_macaddr(peer);
	vdev = wlan_peer_get_vdev(peer);

	if (vdev == NULL) {
		sa_api_err("vdev is NULL!\n");
		return;
	}

	param.vdev_id = wlan_vdev_get_id(vdev);
	pdev = wlan_vdev_get_pdev(vdev);

	if (pdev == NULL) {
		sa_api_err("pdev is NULL!\n");
		return;
	}

	wmi_unified_smart_ant_set_tx_ant_cmd_send(lmac_get_pdev_wmi_handle(pdev), mac, &param);
}

void target_if_sa_api_set_rx_antenna (struct wlan_objmgr_pdev *pdev, uint32_t antenna)
{
	struct smart_ant_rx_ant_params param;

	qdf_mem_set(&param, sizeof(param), 0);
	param.antenna = antenna;

	wmi_unified_smart_ant_set_rx_ant_cmd_send(lmac_get_pdev_wmi_handle(pdev), &param);
}

int target_if_pdev_set_param(struct wlan_objmgr_pdev *pdev, /* can be moved to generic file */
		uint32_t param_id, uint32_t param_value)
{
	struct pdev_params pparam;
	uint32_t pdev_id;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	qdf_mem_set(&pparam, sizeof(pparam), 0);
	pparam.param_id = param_id;
	pparam.param_value = param_value;

	return wmi_unified_pdev_param_send(lmac_get_pdev_wmi_handle(pdev), &pparam, pdev_id);
}

void target_if_sa_api_enable_sa(struct wlan_objmgr_pdev *pdev, uint32_t enable,
				uint32_t mode, uint32_t rx_antenna)
{
	/* Send WMI COMMAND to Enable */
	struct smart_ant_enable_params param;
	int ret;
	void __iomem *smart_antenna_gpio;
	u_int32_t reg_value;
	struct pdev_osif_priv *osif_priv = NULL;
	struct wlan_objmgr_psoc *psoc;
	bool is_ar900b;
	uint32_t target_type;
	ol_ath_soc_softc_t *soc;

	qdf_mem_set(&param, sizeof(param), 0);
	param.enable = enable & SMART_ANT_ENABLE_MASK;
	param.mode = mode;
	param.rx_antenna = rx_antenna;

	param.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	osif_priv = wlan_pdev_get_ospriv(pdev);
	psoc = wlan_pdev_get_psoc(pdev);

	is_ar900b = lmac_is_target_ar900b(psoc);
	target_type = lmac_get_tgt_type(psoc);

        if (!is_ar900b) {
		param.gpio_pin[0] = OL_SMART_ANTENNA_PIN0;
		param.gpio_func[0] = OL_SMART_ANTENNA_FUNC0;
		param.gpio_pin[1] = OL_SMART_ANTENNA_PIN1;
		param.gpio_func[1] = OL_SMART_ANTENNA_FUNC1;
		param.gpio_pin[2] = OL_SMART_ANTENNA_PIN2;
		param.gpio_func[2] = OL_SMART_ANTENNA_FUNC2;
		param.gpio_pin[3] = 0;  /*NA for !is_ar900b */
		param.gpio_func[3] = 0; /*NA for !is_ar900b */
	}

	if (enable && target_type == TARGET_TYPE_IPQ4019) {

		/* Enable Smart antenna related GPIOs */
		smart_antenna_gpio = ioremap_nocache(IPQ4019_SMARTANTENNA_BASE_GPIO, IPQ4019_SMARTANTENNA_GPIOS_REG_SIZE);

		if (smart_antenna_gpio) {
			if (wlan_pdev_in_gmode(pdev)) {
				qdf_print("Enabling 2G Smart Antenna GPIO on ipq4019");
				reg_value = A_IO_READ32(smart_antenna_gpio);
				reg_value = (reg_value & ~0x1C) | 0xC;
				A_IO_WRITE32(smart_antenna_gpio, reg_value); /* gpio 44 2G Strobe */

				reg_value = A_IO_READ32(smart_antenna_gpio + IPQ4019_SMARTANTENNA_GPIO45_OFFSET);
				reg_value = (reg_value & ~0x1C) | 0x10;
				A_IO_WRITE32(smart_antenna_gpio+0x1000, reg_value); /* gpio 45 2G Sdata */
			} else {
				qdf_print("Enabling 5G Smart Antenna GPIO on ipq4019");
				reg_value = A_IO_READ32(smart_antenna_gpio + IPQ4019_SMARTANTENNA_GPIO46_OFFSET);
				reg_value = (reg_value & ~0x1C) | 0xC;
				A_IO_WRITE32(smart_antenna_gpio+0x2000, reg_value); /* gpio 46 5G Strobe */

				reg_value = A_IO_READ32(smart_antenna_gpio + IPQ4019_SMARTANTENNA_GPIO47_OFFSET);
				reg_value = (reg_value & ~0x1C) | 0xC;
				A_IO_WRITE32(smart_antenna_gpio+0x3000, reg_value); /* gpio 47 5G Sdata */
			}

			iounmap(smart_antenna_gpio);
		}

	}

	if (!enable) {
		/* Disable Smart antenna related GPIOs */
		if (target_type == TARGET_TYPE_IPQ4019) {

			smart_antenna_gpio = ioremap_nocache(IPQ4019_SMARTANTENNA_BASE_GPIO, IPQ4019_SMARTANTENNA_GPIOS_REG_SIZE);

			if (smart_antenna_gpio) {
				if (wlan_pdev_in_gmode(pdev)) {
					qdf_print ("Disabling 2G Smart Antenna GPIO on ipq4019\n");
					reg_value = A_IO_READ32(smart_antenna_gpio);
					reg_value = (reg_value & ~0x1C);
					A_IO_WRITE32(smart_antenna_gpio, reg_value); /* gpio 44 2G Strobe */

					reg_value = A_IO_READ32(smart_antenna_gpio + 0x1000);
					reg_value = (reg_value & ~0x1C);
					A_IO_WRITE32(smart_antenna_gpio + 0x1000, reg_value); /* gpio 45 2G Sdata */
				} else {
					qdf_print("Disabling 5G Smart Antenna GPIO on ipq4019");
					reg_value = A_IO_READ32(smart_antenna_gpio + 0x2000);
					reg_value = (reg_value & ~0x1C);
					A_IO_WRITE32(smart_antenna_gpio+0x2000, reg_value); /* gpio 46 5G Strobe */

					reg_value = A_IO_READ32(smart_antenna_gpio + 0x3000);
					reg_value = (reg_value & ~0x1C);
					A_IO_WRITE32(smart_antenna_gpio + 0x3000, reg_value); /* gpio 47 5G Sdata */
				}
				iounmap(smart_antenna_gpio);
			}
		}
	}

	/* Enable txfeed back to receive TX Control and Status descriptors from target */
	ret = wmi_unified_smart_ant_enable_cmd_send(lmac_get_pdev_wmi_handle(pdev), &param);
	if (ret == 0) {
                if (is_ar900b) {
			if (enable) {
				if (target_if_sa_api_is_tx_feedback_enabled(psoc, pdev)) {
					ret = target_if_pdev_set_param(pdev,
							wmi_pdev_param_en_stats, 1);
					if (ret != EOK) {
						qdf_print("Enabling txfeedback Stats failed ");
					}
				}
			} else {
				ret = target_if_pdev_set_param(pdev,
						wmi_pdev_param_en_stats, 0);
				if (ret != EOK) {
					qdf_print("Diabling txfeedback Stats failed ");
				}
			}
		} else {
			soc = (ol_ath_soc_softc_t *)lmac_get_psoc_feature_ptr(psoc);
			if (enable) {
				if (target_if_sa_api_is_tx_feedback_enabled(psoc, pdev)) {
					if(soc && soc->ol_if_ops->smart_ant_enable_txfeedback)
						soc->ol_if_ops->smart_ant_enable_txfeedback(pdev, 1);
				}
			} else {
				if(soc && soc->ol_if_ops->smart_ant_enable_txfeedback)
					soc->ol_if_ops->smart_ant_enable_txfeedback(pdev, 0);
			}
		}
	}
}

void target_if_sa_set_training_info(struct wlan_objmgr_peer *peer,
					uint32_t *rate_array,
					uint32_t *antenna_array,
					uint32_t numpkts)
{
	struct smart_ant_training_info_params param;
	uint8_t *mac = NULL;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;

	qdf_mem_set(&param, sizeof(param), 0);

	mac = wlan_peer_get_macaddr(peer);
	vdev = wlan_peer_get_vdev(peer);

	param.vdev_id = wlan_vdev_get_id(vdev);
	pdev = wlan_vdev_get_pdev(vdev);


	param.numpkts = numpkts;
	param.rate_array = rate_array;
	param.antenna_array = antenna_array;

	wmi_unified_smart_ant_set_training_info_cmd_send(lmac_get_pdev_wmi_handle(pdev), mac,
			&param);
}

void target_if_sa_api_set_peer_config_ops(struct wlan_objmgr_peer *peer,
					uint32_t cmd_id, uint16_t args_count,
					u_int32_t args_arr[])
{
	struct smart_ant_node_config_params param;
	uint8_t *mac = NULL;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;

	qdf_mem_set(&param, sizeof(param), 0);

	mac = wlan_peer_get_macaddr(peer);
	vdev = wlan_peer_get_vdev(peer);

	param.vdev_id = wlan_vdev_get_id(vdev);
	pdev = wlan_vdev_get_pdev(vdev);

	param.cmd_id = cmd_id;
	param.args_count = args_count;
	param.args_arr = args_arr;

	wmi_unified_smart_ant_node_config_cmd_send(lmac_get_pdev_wmi_handle(pdev), mac, &param);
}

void target_if_sa_api_tx_ops_register(struct wlan_lmac_if_tx_ops *tx_ops)
{
	tx_ops->sa_api_tx_ops.sa_api_register_event_handler =
				target_if_sa_api_register_wmi_event_handler;
	tx_ops->sa_api_tx_ops.sa_api_unregister_event_handler =
				target_if_sa_api_unregister_wmi_event_handler;

	tx_ops->sa_api_tx_ops.sa_api_set_tx_antenna =
				target_if_sa_api_set_tx_antenna;
	tx_ops->sa_api_tx_ops.sa_api_set_rx_antenna =
				target_if_sa_api_set_rx_antenna;
	tx_ops->sa_api_tx_ops.sa_api_set_tx_default_antenna =
				target_if_sa_api_set_rx_antenna;

	tx_ops->sa_api_tx_ops.sa_api_enable_sa = target_if_sa_api_enable_sa;

	tx_ops->sa_api_tx_ops.sa_api_set_training_info =
				target_if_sa_set_training_info;
	tx_ops->sa_api_tx_ops.sa_api_set_node_config_ops =
				target_if_sa_api_set_peer_config_ops;
}

