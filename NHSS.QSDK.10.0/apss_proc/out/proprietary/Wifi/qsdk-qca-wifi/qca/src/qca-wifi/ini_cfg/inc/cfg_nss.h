/*
 *
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _CFG_NSS_H_
#define _CFG_NSS_H_

#include "cfg_define.h"

#define CFG_NSS_NEXT_HOP CFG_INI_BOOL("nss_wifi_nxthop_cfg", false, \
		"NSS Wifi next hop configuration")

#define CFG_NSS_WIFI_OL CFG_INI_UINT("nss_wifi_olcfg", 0x00 , 0x07 , 0x00, \
		CFG_VALUE_OR_DEFAULT, "NSS Wifi Offload Configuration")

#define CFG_NSS_WIFILI_OL CFG_INI_UINT("nss_wifili_olcfg", 0x00 , 0x07 , 0x00, \
		CFG_VALUE_OR_DEFAULT, "NSS Wifi Offload Configuration")

#define CFG_NSS \
		CFG(CFG_NSS_NEXT_HOP) \
		CFG(CFG_NSS_WIFI_OL) \
		CFG(CFG_NSS_WIFILI_OL)


#endif /* _CFG_NSS_H_ */
