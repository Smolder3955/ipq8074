/*
* Copyright (c) 2012, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _DFS_DOMAINS_H_
#define _DFS_DOMAINS_H_

/*
 * This definition MUST match what's in the HAL(hal/ah_internal.h) for now
 */
typedef enum {
    DFS_UNINIT_DOMAIN   = 0,    /* Uninitialized dfs domain */
    DFS_FCC_DOMAIN      = 1,    /* FCC3 dfs domain */
    DFS_ETSI_DOMAIN     = 2,    /* ETSI dfs domain */
    DFS_MKK4_DOMAIN = 3,    /* Japan dfs domain */
} DFS_DOMAIN;

#endif
