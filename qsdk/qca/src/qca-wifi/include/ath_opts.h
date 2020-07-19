/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */


#ifndef ATH_OPTS_H
#define ATH_OPTS_H

#include <wlan_opts.h>

/*
 * default definitions for optional modules.
 */


/*
 * if the HT support is on then turn on force ppm by default.
 */

#ifndef ATH_SUPPORT_FORCE_PPM
#define ATH_SUPPORT_FORCE_PPM 0
#endif

#define ATH_SUPPORT_FORCE_PPM 0


/*
 * if LED support is not defined , turn it on by default
 */
#ifndef ATH_SUPPORT_LED
#define ATH_SUPPORT_LED 1
#endif

/*
 * if Power save support is not defined , turn it on by default
 */
#ifndef ATH_SUPPORT_POWER
#define ATH_SUPPORT_POWER 1
#endif

/*
 * if Enhanced DMA (EDMA) support is not defined , turn it on by default
 * This feature depends on AH_SUPPORT_AR9300 being defined in HAL.
 */
#ifndef ATH_SUPPORT_EDMA
#define ATH_SUPPORT_EDMA 1
#endif

/*
 * STBC support
 */
#ifndef ATH_SUPPORT_STBC
#define ATH_SUPPORT_STBC 0
#endif

/*
 * EZCONNECT support
 */
#ifndef ATH_SUPPORT_EZCONNECT
#define ATH_SUPPORT_EZCONNECT 0
#endif

/*
** Default support for VLAN OFF
*/

#ifndef ATH_SUPPORT_VLAN
#define ATH_SUPPORT_VLAN 0
#endif

#ifndef ATH_SUPPORT_RAW_ADC_CAPTURE
#define ATH_SUPPORT_RAW_ADC_CAPTURE 0
#endif

/*
 * Allocate descriptors in cachable memory
 * It is turned on by default for Linux AP build
 * so disable it here for other platforms
 */
#ifndef ATH_SUPPORT_DESC_CACHABLE
#define ATH_SUPPORT_DESC_CACHABLE 0
#endif

#endif
