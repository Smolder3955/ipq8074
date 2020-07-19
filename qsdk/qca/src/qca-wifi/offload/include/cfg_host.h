/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _CFG_HOST__H_
#define _CFG_HOST__H_

#if defined(CONFIG_AR6004_SUPPORT)
#include <wmi.h> /* NUM_DEV, AP_MAX_NUM_STA */
#elif defined(CONFIG_AR6320_SUPPORT) /* TODO: use AR6004's content to compile, fix this later */
#include <wmi.h> /* NUM_DEV, AP_MAX_NUM_STA */
#endif

#endif /* _CFG_HOST__H_ */
