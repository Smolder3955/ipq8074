/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef __FW_PACK_H__
#define __FW_PACK_H__
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* -- all standard includes above -- */
#include <fw_osdep.h>
#include <fw_util.h>
#include <prd_specific.h>
#include <fw_header.h>

#define unused(__x) (__x) = (__x)
/* prototypes of any functions that are used*/
/*
 * purpose of the fw_pack utility is to create both firmware header and the 
 * signature trailer to the firmware bin file. All required information is 
 * passed on command line to the utility. A global fw_header structure is created 
 * and written to new file. 
 * Either entire firmware file can be loaded in memory and then the memory can 
 * be written to file, or in binary format entire header can be written first 
 * then the actual file.  
 *  
 * This tool requires following information 
 *  - firmware filename              - maximum of 1024 characters of input.
                                       optionally with argument -i
 *  - firmware type (bin,ascii/text) - Default bin, unless -a is passed
 *  - target resident                - Default target resident, unless -r (host
 *                                     resident)is firmware is target downloaded
 *                                     or host Resident
 *  - compressed                     - Default uncompressed, -c for compressed
 *  - firmware version string        - version string to drive FW version
 *  - Production/Test                - Default Production, unless specfied as test
 *                                     through flag '-t'
 *  - output file name               - default same file with extra .xsig
 *  - product ID                     - Device name for which file is generated
 *                                     derive rest from there. Option -p
 * In general the fw_pack [-a] [-r] [-c] {-v version_string } [-t] [-o out_file_name] {-p product_id} [-i] input_filename
 * 
 */
 /* fw_pack_cmd_args
  * fw_pack_cmd_args holds global command line arguments, command handles one 
  * file at a time, so no re-entry checks etc. are required.  
  */
struct fw_pack_cmd_args {
    int              in_file_type;                          /* binary, text */
    unsigned int     in_file_size;                      /* size of the file */
    unsigned int     sig_file_size;                  /* signature file size */
    int              t_resident;         /* target resident or host resident*/
    int              compressed;               /* is compressed, default no */
    int              fw_release;                      /* production or test */
    int              prd_dev_id;                 /* device id to check with */
    char             fw_ver_string[256];         /* firmware version string */
    char            *ofile;                             /* output file name */
    char            *in_file;                             /* input file name*/
    char            *sig_file;                            /* signature file */
    int             test_file;
    /* extra arguments used to communicate with rest of the functions
     * to reduce number of paramters exchange
     */
    fw_img_file_magic_t img_type;
    int             header_only;
    int             sign_only;
    
};
enum {
    FW_PACK_ERR_INTERNAL,
    FW_PACK_ERR_INV_VER_STRING,
    FW_PACK_ERR_INV_DEV_ID,
    FW_PACK_ERR_INPUT,
    FW_PACK_ERR_INPUT_FILE,
    FW_PACK_ERR_FILE_FORMAT,
    FW_PACK_ERR_FILE_ACCESS,
    FW_PACK_ERR_CREATE_OUTPUT_FILE,
    FW_PACK_ERR_UNSUPP_CHIPSET,
    FW_PACK_ERR_FILE_WRITE,
    FW_PACK_ERR_INVALID_FILE_MAGIC,
    FW_PACK_ERR_INFILE_READ,
    FW_PACK_ERR_IMAGE_VER,
    FW_PACK_ERR_SIGN_ALGO,
    FW_PACK_ERR_MAX
};
struct auth_input {
	unsigned char *certBuffer;
	unsigned char *signature;
	unsigned char *data;
	int cert_len;
	int sig_len;
	int data_len;
	int sig_hash_algo; /* Hash Algorithm used for signature */
	int pk_algo; /* Public Key Algorithm */;
	int cert_type; /* Type of Public Key Certificate */
};

int test_authenticate_fw(struct auth_input *ai);
#endif /* end __FW_PACK_H__ */
