/*
 *
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _CFG_ALL_H_
#define _CFG_ALL_H_

#include "cfg_ol.h"
#ifdef WLAN_SA_API_ENABLE
#include "cfg_sa.h"
#endif
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include "cfg_nss.h"
#endif
#include "cfg_converged.h"
#include "cfg_target.h"

#if !defined(CFG_SA)
#define CFG_SA
#endif

#if !defined(CFG_NSS)
#define CFG_NSS
#endif


#define CFG_ALL \
		CFG_OL \
		CFG_SA \
		CFG_NSS \
		CFG_CONVERGED_ALL \
		CFG_TARGET

#endif /*_CFG_ALL_H_*/

