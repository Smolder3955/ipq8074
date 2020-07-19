/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _ATF_OL_API_I_H_
#define _ATF_OL_API_I_H_

#include "atf_defs_i.h"

/**
 * atf_ctx_init_ol() - Internal function to initialise ATF context
 * with offload specific functions
 * @ac : ATF context
 *
 * Return : void
 */
void atf_ctx_init_ol(struct atf_context *ac);

#endif /* _ATF_OL_API_I_H_ */
