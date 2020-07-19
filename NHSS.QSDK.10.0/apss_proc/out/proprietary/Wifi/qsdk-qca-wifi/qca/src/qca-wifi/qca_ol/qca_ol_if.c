/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include "qca_ol_if.h"
#include "target_type.h"
#include <init_deinit_lmac.h>

/* Function pointer to call DA/OL specific tx_ops registration function */
struct ol_if_offload_ops *ol_if_offload_ops_register[OL_WIFI_TYPE_MAX];

/**
 * ol_if_offload_ops_attach() - offload ops registration
 * callback assignment
 * @dev_type: Dev type can be either offlaod WIFI2.0 & WIFI3.0
 * @offload_ops: Offload ops registration
 *
 * API to assign appropriate ops registration based on the
 * device type
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS ol_if_offload_ops_attach(ol_ath_soc_softc_t *soc, uint32_t target_type)
{
    if ((target_type == TARGET_TYPE_QCA6290) ||
		   (target_type == TARGET_TYPE_QCA8074) ||
		   (target_type == TARGET_TYPE_QCA8074V2) ||
		   (target_type == TARGET_TYPE_QCA6018))
	soc->ol_if_ops = ol_if_offload_ops_register[OL_WIFI_3_0];
    else
	soc->ol_if_ops = ol_if_offload_ops_register[OL_WIFI_2_0];

    return QDF_STATUS_SUCCESS;
}

/**
 * ol_if_offload_ops_registration() - offload ops registration
 * callback assignment
 * @dev_type: Dev type can be either offlaod WIFI2.0 & WIFI3.0
 * @offload_ops: Offload ops registration
 *
 * API to assign appropriate ops registration based on the
 * device type
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS ol_if_offload_ops_registration(OL_WIFI_DEV_TYPE dev_type,
			struct ol_if_offload_ops *offload_ops)
{
	if (dev_type < OL_WIFI_TYPE_MAX)
		ol_if_offload_ops_register[dev_type] = offload_ops;

	qdf_debug("offload ops registration for wifi:%d with ops=%pK",
			dev_type, offload_ops);

	return QDF_STATUS_SUCCESS;
}
qdf_export_symbol(ol_if_offload_ops_registration);
