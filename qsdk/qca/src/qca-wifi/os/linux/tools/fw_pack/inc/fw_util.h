/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * All utility functions
 */
#include <fw_osdep.h>
/* generic useful macros */
#ifndef NELEMENTS
#define NELEMENTS(__array) sizeof((__array))/sizeof ((__array)[0])
#endif

#define ALIGNTO(__addr, __to) ((((unsigned long int)(__addr)) + (__to) -  1 ) & ~((__to) - 1))
#define IS_ALIGNED(__addr, __to) (!(((unsigned long int)(__addr)) & ((__to)-1)))
