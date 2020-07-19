/*
 * Copyright (c) 2015-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
 * osif_nss_wifiol_cmn.c
 *
 * This file used for for interface   NSS WiFi Offload Radio
 * ------------------------REVISION HISTORY-----------------------------
 * Qualcomm Atheros         4/jan/2018              Created
 */

#include <ol_if_athvar.h>

#include <nss_api_if.h>
#include <nss_cmn.h>
#include <hif.h>
#include "osif_nss_wifiol_vdev_if.h"
#include "osif_nss_wifiol_if.h"
#include "target_type.h"
#include "qca_ol_if.h"
#include "init_deinit_lmac.h"
#include "cfg_ucfg_api.h"

int glbl_allocated_radioidx=0;
qdf_export_symbol(glbl_allocated_radioidx);
int global_device_id = 0;
int nssdevice_id = 0;

static struct nss_wifi_soc_ops *nss_wifi_soc_register[OL_WIFI_TYPE_MAX];

/*
 * osif_nss_ol_assign_ifnum : Get NSS IF Num based on Radio ID
 */

int osif_nss_ol_assign_ifnum(int radio_id, ol_ath_soc_softc_t *soc, bool is_2g) {

	uint32_t i = 0;
	uint32_t found_idx = 0;
	uint32_t start_idx = 0;

	if (is_2g) {
		start_idx = 1;
	}

	for (i = start_idx; i < 3; i++) {
		if ((glbl_allocated_radioidx & (1 << i)) == 0) {
			glbl_allocated_radioidx |= (1 << i);
			found_idx = 1;
			break;
		}
	}

	if (!found_idx) {
		qdf_print("%s: Unable to allocate nss interface is_2g %d radioidx val %x startidx %x", __FUNCTION__, is_2g, glbl_allocated_radioidx, start_idx);
		soc->nss_soc.nss_sidx = -1;
		return -1;
	}

	soc->nss_soc.nss_sidx = i;

	switch (i) {
		case 0:
			return NSS_WIFI_INTERFACE0;

		case 1:
			return NSS_WIFI_INTERFACE1;

		case 2:
			return NSS_WIFI_INTERFACE2;
	}

	return -1;

}

void osif_nss_register_module(OL_WIFI_DEV_TYPE target_type,
			struct nss_wifi_soc_ops *soc_ops)
{
    if (target_type < OL_WIFI_TYPE_MAX) {
        nss_wifi_soc_register[target_type] = soc_ops;
	qdf_print("NSS wifi ops registered for target_type:%d with soc_ops:%pK",
			target_type, soc_ops);
    }

	return;
}
qdf_export_symbol(osif_nss_register_module);

/**
 * osif_nss_wifi_soc_setup() - soc setup
 * @soc : soc handle
 */
void osif_nss_wifi_soc_setup(ol_ath_soc_softc_t *soc)
{
    uint32_t target_type = lmac_get_tgt_type(soc->psoc_obj);
    uint8_t radio_cnt = 1;
    enum wmi_host_hw_mode_config_type preferred_hw_mode = lmac_get_preferred_hw_mode(soc->psoc_obj);
    uint32_t nss_soc_cfg = cfg_get(soc->psoc_obj, CFG_NSS_WIFI_OL);

    /*
     * NSS offload mode is enabled by default for 6018 emulation validation.
     * This has to be removed once emulation validation is completed.
     */
    if (target_type  == TARGET_TYPE_QCA6018) {
        nss_soc_cfg = 1;
    }

    soc->nss_soc.nss_wifiol_id = -1;
    soc->nss_soc.ops = NULL;
    if (nss_cmn_get_nss_enabled() == true) {
        if (nss_soc_cfg & (1 << global_device_id )) {

            if ((target_type == TARGET_TYPE_AR900B)
                    || (target_type == TARGET_TYPE_QCA9984) ){

                soc->nss_soc.nss_wifiol_id = nssdevice_id;
                soc->nss_soc.nss_sifnum = osif_nss_ol_assign_ifnum(soc->nss_soc.nss_wifiol_id,
                        soc ,(cfg_get(soc->psoc_obj, CFG_NSS_WIFI_OL) >> 16 & (1 << global_device_id)));

                if (soc->nss_soc.nss_sifnum == -1) {
                    soc->nss_soc.nss_wifiol_id = -1;
                    qdf_print("Unable to assign interface number for radio %d", soc->nss_soc.nss_wifiol_id);
                    /* error = -EINVAL; */
                    goto devnotenabled;
                }

                soc->nss_soc.ops = nss_wifi_soc_register[OL_WIFI_2_0];
                if (!soc->nss_soc.ops) {
                    qdf_print("nss-wifi:nss wifi ops is NULL for WIFI2.0 target");
                    /* error = -EINVAL; */
                    goto devnotenabled;
                }

                qdf_print("nss-wifi:#1 register wifi function for soc nss id %d device id %d", nssdevice_id, global_device_id);

                nssdevice_id++;
                qdf_print("nss_wifi_olcfg value is %x", cfg_get(soc->psoc_obj, CFG_NSS_WIFI_OL));
                qdf_print("Got NSS IFNUM as %d", soc->nss_soc.nss_sifnum);

                if(cfg_get(soc->psoc_obj, CFG_NSS_NEXT_HOP) & (1 << global_device_id )) {
                    soc->nss_soc.nss_nxthop = 1;
                }

            } else if ((target_type == TARGET_TYPE_QCA8074) ||
                        (target_type == TARGET_TYPE_QCA8074V2) ||
                        (target_type == TARGET_TYPE_QCA6018)) {
                switch (preferred_hw_mode) {
                    case WMI_HOST_HW_MODE_DBS:
                        soc->nss_soc.nss_scfg = 0x3;
                        radio_cnt = 2;
                        break;
                    case WMI_HOST_HW_MODE_DBS_SBS:
                        soc->nss_soc.nss_scfg = 0x7;
                        radio_cnt = 3;
                        break;
                    case WMI_HOST_HW_MODE_SINGLE:
                        soc->nss_soc.nss_scfg = 0x1;
                        radio_cnt = 1;
                        break;
                    default:
                        qdf_info("nss-wifili: Could not set nss_cfg due to Invalid HW mode %d", preferred_hw_mode);
                        goto devnotenabled;
                }

		soc->nss_soc.ops = nss_wifi_soc_register[OL_WIFI_3_0];
                if (!soc->nss_soc.ops) {
                    qdf_print("nss-wifi:nss wifi ops is NULL for WIFI3.0 target");
                    /* error = -EINVAL; */
                    soc->nss_soc.nss_scfg = 0x0;
                    radio_cnt = 0;
                    goto devnotenabled;
                }

                if (cfg_get(soc->psoc_obj, CFG_NSS_NEXT_HOP)) {
                    soc->nss_soc.nss_nxthop = 1;
                }

                qdf_print("nss-wifili:#1 register wifili function for soc ");
            } else {
                qdf_print("Target type not supported in NSS wifi offload %x", target_type);
            }

        }
        if (soc->nss_soc.ops) {
            soc->nss_soc.ops->nss_soc_wifi_init(soc);
        }
    }
devnotenabled:
    qdf_print("nss register id %d nss config %x Target Type %x ",
            soc->nss_soc.nss_wifiol_id, cfg_get(soc->psoc_obj, CFG_NSS_WIFI_OL), target_type);
    global_device_id+= radio_cnt;
}
