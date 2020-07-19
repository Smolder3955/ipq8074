/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _ATF_DA_API_I_H_
#define _ATF_DA_API_I_H_

#include "atf_defs_i.h"

/**
 * atf_ctx_init_da() - Internal function to initialise ATF context
 * with direct attach specific functions
 * @ac : ATF context
 *
 * Return : void
 */
void atf_ctx_init_da(struct atf_context *ac);

#endif /* _ATF_DA_API_I_H_ */
