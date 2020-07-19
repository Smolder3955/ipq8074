/*
 *
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _CFG_SA_H_
#define _CFG_SA_H_

#include "cfg_define.h"

#define CFG_SA_ENABLE \
		CFG_INI_BOOL("enable_smart_antenna", false, \
		"Enable Smart Antenna")

#define CFG_SA_ENABLE_DA \
		CFG_INI_BOOL("enable_smart_antenna_da", false, \
		"Enable Smart Antenna for Direct Attach")

#define CFG_SA_VALIDATE_SW \
		CFG_INI_UINT("sa_validate_sw", \
		0, 2, 0, CFG_VALUE_OR_DEFAULT, \
		"Validate Smart Antenna Software")

#define CFG_SA \
	CFG(CFG_SA_ENABLE) \
	CFG(CFG_SA_ENABLE_DA) \
	CFG(CFG_SA_VALIDATE_SW)

#endif /* _CFG_SA_H_ */
