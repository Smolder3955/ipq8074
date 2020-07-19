/*
 * Copyright (c) 2015,2016,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015,2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/**
 * DOC: if_fs.h
 * This file provides OS dependent filesystem API's.
 */

#ifndef __I_QDF_FS_H__
#define __I_QDF_FS_H__

#define _GNU_SOURCE
#include <fcntl.h>
extern int  __qdf_fs_read(char *filename, loff_t offset, unsigned int size,
			  unsigned char *buffer);

#endif /*__QDF_FS_PVT_H__*/
