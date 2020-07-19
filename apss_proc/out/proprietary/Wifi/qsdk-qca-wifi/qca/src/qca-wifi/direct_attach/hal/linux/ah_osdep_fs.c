/*
* Copyright (c) 2015-2016, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
* 2015-2016 Qualcomm Atheros, Inc.
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/

#include "opt_ah.h"
#include "ah.h"
#include "ah_internal.h"



#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

/*
** Linux Includes
*/

#include <linux/version.h>
//#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

/**
 * ath_hal_fs_read - a file operation of a kerenl and system
 * @filename: name of file
 * @offset: offset to read file from
 * @size: size of the buffer
 * @buffer: buffer to fill
 *
 * Returns: int
 */
int __ahdecl ath_hal_fs_read(char *filename,
		loff_t offset,
		unsigned int size,
		uint8_t *buffer)
{
	struct file      *filp;
	struct inode     *inode;
	off_t            fsize;
	mm_segment_t     fs;
	ssize_t		ret;

	if (NULL == buffer) {
		HDPRINTF(ah, HAL_DBG_QUEUE,	"%s[%d], Error, null pointer to buffer.", __func__,
				__LINE__);
		return -1;
	}

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		HDPRINTF(ah, HAL_DBG_QUEUE,
				"%s[%d]: Fail to Open File %s", __func__,
				__LINE__, filename);
		return -1;
	}
	HDPRINTF(ah, HAL_DBG_QUEUE,
			"%s[%d], Open File %s SUCCESS!!", __func__,
			__LINE__, filename);

#if 0 /*LINUX_VERSION_CODE < KERNEL_VERSION(3,19,0)*/
	inode = filp->f_dentry->d_inode;
#else
	inode = filp->f_path.dentry->d_inode;
#endif

	HDPRINTF(ah, HAL_DBG_QUEUE,
			"file system magic:%ld", inode->i_sb->s_magic);
	HDPRINTF(ah, HAL_DBG_QUEUE,
			"super blocksize:%ld", inode->i_sb->s_blocksize);
	HDPRINTF(ah, HAL_DBG_QUEUE,
			"inode %ld", inode->i_ino);
	fsize = inode->i_size;
	HDPRINTF(ah, HAL_DBG_QUEUE,
			"file size:%d", (unsigned int)fsize);
	if (fsize != size) {
		HDPRINTF(ah, HAL_DBG_QUEUE,
				"%s[%d]: caldata data size mismatch, fsize=%d, cal_size=%d",
				__func__, __LINE__, (unsigned int)fsize, size);

		if (size > fsize) {
			HDPRINTF(ah, HAL_DBG_QUEUE,
					"%s[%d], exit with error: size > fsize",
					__func__, __LINE__);
			filp_close(filp, NULL);
			return -1;
		}
	}
	fs = get_fs();
	filp->f_pos = offset;
	set_fs(KERNEL_DS);
	ret = vfs_read(filp, (char *)buffer, size, &(filp->f_pos));
	set_fs(fs);
	filp_close(filp, NULL);

	if (ret < 0) {
		HDPRINTF(ah, HAL_DBG_QUEUE,
			"%s[%d]: Fail to Read File %s: %d", __func__,
				 __LINE__, filename, ret);

		return -1;
	}
	return 0;
}
EXPORT_SYMBOL(ath_hal_fs_read);

