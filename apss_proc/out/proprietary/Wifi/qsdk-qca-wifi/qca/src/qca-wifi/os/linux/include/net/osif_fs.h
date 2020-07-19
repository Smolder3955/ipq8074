/*
 * Copyright (c) 2015-2016, 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/**
 * DOC: osif_fs.h
 * This file provides OS abstraction filesystem API's.
 */

#ifndef __QDF_FS_H__
#define __QDF_FS_H__

#include <qdf_types.h>
#include <if_fs.h>

/*
 * @brief a file operation of a kernel and system
 */

/**
 * qdf_fs_read - Read a segment of data from file system to memory buffer
 * @filename: the file name (with including file path) to be read
 * @offset: from position in file, unit is bytes
 * @size: bytes conter to be got
 * @buffer: a buffer pointer to save data to be read
 *
 * @return - returns ERROR (-1) if fail, or OK (0) if success
 */
extern int __ahdecl qdf_fs_read(char *filename,
				loff_t offset,
				unsigned int size,
				unsigned char *buffer);
extern int __ahdecl qdf_fs_write(char *filename,
				 loff_t offset,
				 unsigned int size,
				 unsigned char* buffer);

#endif /*__QDF_FS_H__*/
