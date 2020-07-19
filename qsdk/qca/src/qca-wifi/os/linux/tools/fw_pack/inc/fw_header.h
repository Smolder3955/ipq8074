/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
//#include "os_types.h"
typedef uint fw_img_magic_t;
/* current version of the firmware signing , major 1, minor 0*/
#define THIS_FW_IMAGE_VERSION ((1<<15) | 0) 

enum {
    RSA_PSS1_SHA256 = 1
};
/* fw_img_file_magic
 * In current form of firmware download process, there are three different
 * kinds of files, that get downloaded. All of these can be in different 
 * formats. 
 *
 * 1. downloadable, executable, target resident (athwlan.bin)
 * 2. downloadable, executable, target non-resident otp (otp.bin)
 * 3. downloadable, non-executable, target file (fakeBoard)
 * 4. no-download, no-execute, host-resident code swap file.
 * 5. Add another for future
 * Each of these can be signed or unsigned, each of these can signed 
 * with single key or can be signed with multiple keys, provisioning
 * all kinds in this list.
 * This list contains only filenames that are generic. Each board might
 * name their boards differently, but they continue to use same types.
 */
#define FW_IMG_FILE_MAGIC_TARGET_WLAN       0x5457414eu  /* target WLAN - TWLAN*/
#define FW_IMG_FILE_MAGIC_TARGET_OTP        0x544f5450u    /* target OTP TOTP */
#define FW_IMG_FILE_MAGIC_TARGET_BOARD_DATA 0x54424446u         /* target TBDF*/
#define FW_IMG_FILE_MAGIC_TARGET_CODE_SWAP  0x4857414eu         /* host HWLAN */
#define FW_IMG_FILE_MAGIC_INVALID           0

typedef uint fw_img_file_magic_t;

/* fw_img_sign_ver
 *
 * This supports for multiple revisions of this module to support different
 * features in future. This is defined in three numbers, major and minor 
 * major - In general this would not change, unless, there is new header types
 *         are added or major crypto algorithms changed
 * minor - Minor changes like, adding new devices, new files etc. 
 * There is no sub-release version for this, probably change another minor 
 * version if really needs a change
 * All these are listed  in design document, probably in .c files
 */

/* fw_ver_rel_type
 * FW version release could be either test, of production, zero is not allowed 
 */
#define FW_IMG_VER_REL_TEST         0x01
#define FW_IMG_VER_REL_PROD         0x02
#define FW_IMG_VER_REL_UNDEFIND     0x00

/*
 * fw_ver_maj, fw_ver_minor
 * major and minor versions are planned below. 
 * uint32  fw_ver_maj;                        * 32bits, MMMM.MMMM.SCEE.1111 *
 * uint32  fw_ver_minor;                      * 32bits, mmmm.mmmm.rrrr.rrrr *
 */
/* 
 * extrat major version number from fw_ver_maj field 
 * Higher 16 bits of 32 bit quantity
 * FW_VER_GET_MAJOR - Extract Major version
 * FW_VER_SET_MAJOR - Clear the major version bits and set the bits
 */
#define FW_VER_MAJOR_MASK 0xffff0000u
#define FW_VER_MAJOR_SHIFT 16
#define FW_VER_GET_MAJOR(__mj)  (((__mj)->fw_ver_maj & FW_VER_MAJOR_MASK) >> FW_VER_MAJOR_SHIFT)
#define FW_VER_SET_MAJOR(__mj, __val)  (__mj)->fw_ver_maj =\
                        ((((__val) & 0x0000ffff) << FW_VER_MAJOR_SHIFT) |\
                        ((__mj)->fw_ver_maj  & ~FW_VER_MAJOR_MASK))

/* 
 * Extract build variants. The following variants are defined at this moement.
 * This leaves out scope for future types.
 * This is just a number, so this can contain upto 255 values, 0 is undefined
 */
#define FW_VER_IMG_TYPE_S_RETAIL        0x1U
#define FW_VER_IMG_TYPE_E_ENTERPRISE    0x2U
#define FW_VER_IMG_TYPE_C_CARRIER       0x3U
#define FW_VER_IMG_TYPE_X_UNDEF         0x0U

#define FW_VER_IMG_TYPE_MASK            0x0000ff00
#define FW_VER_IMG_TYPE_SHIFT           8
#define FW_VER_GET_IMG_TYPE(__t) (((__t)->fw_ver_maj & FW_VER_IMG_TYPE_MASK) >>\
                                            FW_VER_IMG_TYPE_SHIFT)
#define FW_VER_SET_IMG_TYPE(__t, __val) \
        (__t)->fw_ver_maj  = \
            ((__t)->fw_ver_maj &~FW_VER_IMG_TYPE_MASK) | \
                ((((uint32)(__val)) & 0xff) << FW_VER_IMG_TYPE_SHIFT)

#define FW_VER_IMG_TYPE_VER_MASK            0x000000ff
#define FW_VER_IMG_TYPE_VER_SHIFT           0

#define FW_VER_GET_IMG_TYPE_VER(__t) (((__t)->fw_ver_maj & \
                     FW_VER_IMG_TYPE_VER_MASK) >> FW_VER_IMG_TYPE_VER_SHIFT)

#define FW_VER_SET_IMG_TYPE_VER(__t, __val) (__t)->fw_ver_maj = \
                         ((__t)->fw_ver_maj&~FW_VER_IMG_TYPE_VER_MASK) |\
                         ((((uint32)(__val)) & 0xff) << FW_VER_IMG_TYPE_VER_SHIFT)

#define FW_VER_IMG_MINOR_VER_MASK           0xffff0000
#define FW_VER_IMG_MINOR_VER_SHIFT          16
#define FW_VER_IMG_MINOR_SUBVER_MASK        0x0000ffff
#define FW_VER_IMG_MINOR_SUBVER_SHIFT       0

#define FW_VER_IMG_MINOR_RELNBR_MASK        0x0000ffff
#define FW_VER_IMG_MINOR_RELNBR_SHIFT       0

#define FW_VER_IMG_GET_MINOR_VER(__m) (((__m)->fw_ver_minor &\
                                        FW_VER_IMG_MINOR_VER_MASK) >>\
                                            FW_VER_IMG_MINOR_VER_SHIFT)

#define FW_VER_IMG_SET_MINOR_VER(__t, __val) (__t)->fw_ver_minor = \
                     ((__t)->fw_ver_minor &~FW_VER_IMG_MINOR_VER_MASK) |\
                     ((((uint32)(__val)) & 0xffff) << FW_VER_IMG_MINOR_VER_SHIFT)

#define FW_VER_IMG_GET_MINOR_SUBVER(__m) (((__m)->fw_ver_minor & \
                     FW_VER_IMG_MINOR_SUBVER_MASK) >> FW_VER_IMG_MINOR_SUBVER_SHIFT)

#define FW_VER_IMG_SET_MINOR_SUBVER(__t, __val) (__t)->fw_ver_minor = \
                     ((__t)->fw_ver_minor&~FW_VER_IMG_MINOR_SUBVER_MASK) |\
                     ((((uint32)(__val)) & 0xffff) << FW_VER_IMG_MINOR_SUBVER_SHIFT)

#define FW_VER_IMG_GET_MINOR_RELNBR(__m) (((__m)->fw_ver_bld_id &\
                     FW_VER_IMG_MINOR_RELNBR_MASK) >> FW_VER_IMG_MINOR_RELNBR_SHIFT)

#define FW_VER_IMG_SET_MINOR_RELNBR(__t, __val) (__t)->fw_ver_bld_id = \
                     ((__t)->fw_ver_bld_id &~FW_VER_IMG_MINOR_RELNBR_MASK) |\
                     ((((uint32)(__val)) & 0xffff) << FW_VER_IMG_MINOR_RELNBR_SHIFT)

/* signed/unsigned - bit 0 of fw_hdr_flags */
#define FW_IMG_FLAGS_SIGNED                 0x00000001U
#define FW_IMG_FLAGS_UNSIGNED               0x00000000U
#define FW_IMG_IS_SIGNED(__phdr) ((__phdr)->fw_hdr_flags & FW_IMG_FLAGS_SIGNED)

/* file format type - bits 1,2,3*/
#define FW_IMG_FLAGS_FILE_FORMAT_MASK       0x0EU
#define FW_IMG_FLAGS_FILE_FORMAT_SHIFT      0x1U
#define FW_IMG_FLAGS_FILE_FORMAT_TEXT       0x1U
#define FW_IMG_FLAGS_FILE_FORMAT_BIN        0x2U
#define FW_IMG_FLAGS_FILE_FORMAT_UNKNOWN    0x0U
#define FW_IMG_FLAGS_FILE_FORMAT_GET(__flags) \
                    (((__flags) & FW_IMG_FLAGS_FILE_FORMAT_MASK) >> \
                    FW_IMG_FLAGS_FILE_FORMAT_SHIFT)

#define FW_IMG_FLAGS_FILE_COMPRES_MASK       0x10U
#define FW_IMG_FLAGS_FILE_COMPRES_SHIFT      0x4U
#define FW_IMG_FLAGS_COMPRESSED             0x1U
#define FW_IMG_FLAGS_UNCOMPRESSED           0x0U
#define FW_IMG_IS_COMPRESSED(__flags) ((__flags)&FW_IMG_FLAGS_FILE_COMPRES_MASK)
#define FW_IMG_FLAGS_SET_COMRESSED(__flags) \
                            ((__flags) |= 1 << FW_IMG_FLAGS_FILE_COMPRES_SHIFT)

#define FW_IMG_FLAGS_FILE_ENCRYPT_MASK       0x20U
#define FW_IMG_FLAGS_FILE_ENCRYPT_SHIFT      0x5U
#define FW_IMG_FLAGS_ENCRYPTED               0x1U
#define FW_IMG_FLAGS_UNENCRYPTED             0x0U
#define FW_IMG_IS_ENCRYPTED(__flags) ((__flags)&FW_IMG_FLAGS_FILE_ENCRYPT_MASK)
#define FW_IMG_FLAGS_SET_ENCRYPT(__flags) \
                            ((__flags) |= 1 << FW_IMG_FLAGS_FILE_ENCRYPT_SHIFT)

/* any file that is dowloaded is marked target resident
 * any file that is not downloaded but loaded at host
 * would be marked NOT TARGET RESIDENT file, or host resident file 
 */ 
#define FW_IMG_FLAGS_FILE_TARGRES_MASK       0x40U
#define FW_IMG_FLAGS_FILE_TARGRES_SHIFT      0x6U
#define FW_IMG_FLAGS_TARGRES                 0x1U
#define FW_IMG_FLAGS_HOSTRES                 0x0U

#define FW_IMG_IS_TARGRES(__flags) ((__flags)&FW_IMG_FLAGS_FILE_TARGRES_MASK)

#define FW_IMG_FLAGS_SET_TARGRES(__flags) \
                   ((__flags) |= (1 <<FW_IMG_FLAGS_FILE_TARGRES_SHIFT))

#define FW_IMG_FLAGS_FILE_EXEC_MASK       0x80U
#define FW_IMG_FLAGS_FILE_EXEC_SHIFT      0x7U
#define FW_IMG_FLAGS_EXEC                 0x1U
#define FW_IMG_FLAGS_NONEXEC                 0x0U
#define FW_IMG_IS_EXEC(__flags) ((__flags)&FW_IMG_FLAGS_FILE_EXEC_MASK)
#define FW_IMG_FLAGS_SET_EXEC (__flags) \
                            ((__flags) |= 1 << FW_IMG_FLAGS_FILE_EXEC_SHIFT)

/* signing algorithms, only rsa-pss1 with sha256 is supported*/
enum {
    FW_IMG_SIGN_ALGO_UNSUPPORTED = 0,
    FW_IMG_SIGN_ALGO_RSAPSS1_SHA256  = 1
};
/* header of the firmware file, also contains the pointers to the file itself
 * and the signature at end of the file
 */
struct firmware_head {
    fw_img_magic_t      fw_img_magic_number;       /* product magic, eg, BLNR */
    uint32              fw_chip_identification;/*firmware chip identification */

    /* boarddata, otp, swap, athwlan.bin etc */
    fw_img_file_magic_t fw_img_file_magic;
    uint16              fw_img_sign_ver;            /* signing method version */

    uint16              fw_img_spare1;                          /* undefined. */
    uint32              fw_img_spare2;                           /* undefined */

    /* Versioning and release types */
    uint32              fw_ver_rel_type;                  /* production, test */
    uint32              fw_ver_maj;            /* 32bits, MMMM.MMMM.SSEE.1111 */
    uint32              fw_ver_minor;          /* 32bits, mmmm.mmmm.ssss.ssss */
    uint32              fw_ver_bld_id;                  /* actual build id */
    /* image versioning is little tricky to handle. We assume there are three 
     * different versions that we can encode.
     * MAJOR - 16 bits, lower 16 bits of this is spare, chip version encoded
               in lower 16 bits
     * minor - 16 bits, 0-65535, 
     * sub-release - 16 bits, usually this would be zero. if required we
     * can use this. For eg. BL.2_1.400.2, is, Beeliner, 2.0, first major 
     * release, build 400, and sub revision of 2 in the same build
     */
    uint32             fw_ver_spare3;

     /* header identificaiton */
    uint32              fw_hdr_length;                       /* header length */
    uint32              fw_hdr_flags;                    /* extra image flags */
    /* image flags are different single bit flags of different characterstics
    // At this point of time these flags include below, would extend as 
    // required
    //                      signed/unsigned, 
    //                      bin/text, 
    //                      compressed/uncompressed, 
    //                      unencrypted,
    //                      target_resident/host_resident, 
    //                      executable/non-execuatable
    */
    uint32              fw_hdr_spare4;                          /* future use */

    uint32              fw_img_size;                           /* image size; */
    uint32              fw_img_length;                /* image length in byes */
    /* there is no real requirement for keeping the size, the size is 
     * fw_img_size = fw_img_length - ( header_size + signature)
     * in otherwords fw_img_size is actual image size that would be
     * downloaded to target board.
     */
    uint32              fw_spare5;
    uint32              fw_spare6;

    /* security details follows here after */
    uint16              fw_sig_len;     /* index into known signature lengths */
    uint8               fw_sig_algo;                   /* signature algorithm */
    uint8               fw_oem_id;            /* oem ID, to access otp or etc */
    /* actual image body    */
#if 0
    uint8              *fw_img_body;                     /* fw_img_size bytes */
    uint8              *fw_img_padding;          /*if_any_upto_4byte_boundary */
    uint8              *fw_signature;                  /* pointer to checksum */
#endif
} ;
