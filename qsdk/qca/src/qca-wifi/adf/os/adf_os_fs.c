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

#include <adf_os_fs.h>

/*
 * @brief a file operation of a kerenl and system
 */

int __ahdecl adf_os_fs_read(char *filename,
                                loff_t offset,
                                unsigned int size,
                                unsigned char *buffer)
{
    return __adf_os_fs_read(filename, offset, size, buffer);
}

