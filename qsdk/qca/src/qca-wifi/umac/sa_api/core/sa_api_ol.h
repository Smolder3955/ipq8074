/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _SA_API_OL_API_I_H_
#define _SA_API_OL_API_I_H_

#include "sa_api_defs.h"

/**
 * sa_api_ctx_init_ol() - Internal function to initialise sa api context
 * with offload specific functions
 * @sc : sa api context
 *
 * Return : void
 */
void sa_api_ctx_init_ol(struct sa_api_context *sc);

#endif /* _SA_API_OL_API_I_H_ */
