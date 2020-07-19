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



#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <adf_os_fs.h>


/*
 * Read file from file system
 * for now, only for getting calibration data from file
 */
int __adf_os_fs_read(char *filename, loff_t offset, unsigned int size, unsigned char *buffer)
{
    struct file      *filp;
    struct inode     *inode;
    unsigned long    magic;
    off_t            fsize;
    mm_segment_t     fs;

    if(NULL == buffer)
    {
        printk("%s[%d], Error, null pointer to buffer.\n", __func__, __LINE__);
        return -1;
    }

    filp = filp_open(filename, O_RDONLY, 0);
    if (IS_ERR(filp)) {
        printk("%s[%d]: Fail to Open File %s\n", __func__, __LINE__, filename);
        return -1;
    }

    printk("%s[%d], Open File %s SUCCESS!!\n", __func__, __LINE__, filename);

    inode = filp->f_dentry->d_inode;
    magic = inode->i_sb->s_magic;
    printk("file system magic:%ld \n", magic);
    printk("super blocksize:%ld \n", inode->i_sb->s_blocksize);
    printk("inode %ld \n", inode->i_ino);

    fsize = inode->i_size;
    printk("file size:%d \n", (unsigned int)fsize);

    if (fsize != size) {
        printk("%s[%d]: caldata data size mismatch, fsize=%d, cal_size=%d\n",
            __func__, __LINE__, (unsigned int)fsize, size);

        if(size > fsize)
        {
            printk("%s[%d], exit with error: size > fsize\n", __func__, __LINE__);
            filp_close(filp, NULL);
            return -1;
        }
    }

    fs = get_fs();

    filp->f_pos = offset;
    set_fs(KERNEL_DS);
    filp->f_op->read(filp, buffer, size, &(filp->f_pos));
    set_fs(fs);

    filp_close(filp, NULL);

    return 0;

}

EXPORT_SYMBOL(adf_os_fs_read);

