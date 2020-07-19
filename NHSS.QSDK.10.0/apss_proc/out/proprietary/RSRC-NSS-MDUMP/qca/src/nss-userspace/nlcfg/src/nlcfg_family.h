/*
 * Copyright (c) 2019 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef __NLCFG_FAMILY_H
#define __NLCFG_FAMILY_H

#include "nlcfg_ipv4.h"
#include "nlcfg_ipv6.h"

/*
 * Family match params
 */
extern struct nlcfg_param nlcfg_ipv4_params[NLCFG_IPV4_CMD_MAX];
extern struct nlcfg_param nlcfg_ipv6_params[NLCFG_IPV6_CMD_MAX];

#endif /* __NLCFG_FAMILY_H*/

