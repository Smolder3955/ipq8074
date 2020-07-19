/*
* Copyright (c) 2015, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
* 2015 Qualcomm Atheros, Inc.
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/



#ifndef __ADF_OS_FS_H__
#define __ADF_OS_FS_H__

#include <adf_os_types.h>
#include <adf_os_fs_pvt.h>

/*
 * @brief a file operation of a kerenl and system
 */

/**
 * @brief Read a segment of data from file system to memory buffer 
 *
 * @param[in]  filename,     the file name (with including file path) to be read 
 * @param[in]  offset,       from position in file, unit is bytes
 * @param[in]  size,       bytes conter to be got
 * @param[out] buffer,      a buffer pointer to save data to be read
 * 
 * @return     returns ERROR (-1) if fail, or OK (0) if success
 */
extern int __ahdecl adf_os_fs_read(char *filename, 
                                   loff_t offset, 
                                   unsigned int size, 
                                   unsigned char *buffer);

#endif /*__ADF_OS_FS_H__*/
