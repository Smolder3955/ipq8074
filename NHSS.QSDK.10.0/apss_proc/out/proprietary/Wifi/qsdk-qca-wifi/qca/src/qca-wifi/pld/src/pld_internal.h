/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
 */

#ifndef __PLD_COMMON_I_H__
#define __PLD_COMMON_I_H__

#include "pld_common.h"
#include "qdf_lock.h"

struct dev_node {
	struct device *dev;
	struct list_head list;
	enum pld_bus_type bus_type;
};

struct pld_context {
	struct pld_driver_ops *ops;
	qdf_spinlock_t pld_lock;
	struct list_head dev_list;
	uint32_t pld_driver_state;
};

struct pld_context *pld_get_global_context(void);
int pld_add_dev(struct pld_context *pld_context,
		struct device *dev, enum pld_bus_type type);
void pld_del_dev(struct pld_context *pld_context,
		 struct device *dev);

#endif
