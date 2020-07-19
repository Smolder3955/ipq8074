/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _SA_API_DA_API_I_H_
#define _SA_API_DA_API_I_H_

#include "sa_api_defs.h"

/**
 * sa_api_ctx_init_da() - Internal function to initialise SA API context
 * with direct attach specific functions
 * @sc : SA API context
 *
 * Return : void
 */
void sa_api_ctx_init_da(struct sa_api_context *sc);

#endif /* _SA_API_DA_API_I_H_ */
