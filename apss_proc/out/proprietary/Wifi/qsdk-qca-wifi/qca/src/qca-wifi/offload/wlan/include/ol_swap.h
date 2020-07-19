/*
 * Copyright (c) 2011-2014, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ol_if_athvar.h"


#if 0
#include <a_types.h>
#include <a_osapi.h>
#endif

#include "ol_if_athvar.h"


#define EVICT_BIN_MAX_SIZE      (512*1024)    /* Max bin size in bytes */

#define MAX_SWAP_FILENAME 128

#define SWAP_MAX_SEGS 16
/*
 * seg_total_bytes: Total bytes across all the code/data segment blocks
 * num_segs: No of Pages used
 * seg_size: Size of each segment. Should be power of 2 and multiple of 4K
 * seg_size_log2: log2(seg_size)
 * target_swapwrite_addr: Target addr where to write the host swap ddr address
 * seg_busaddr: Physical address of the DMAble memory;4K aligned
 */
struct swap_seg_info {
    u_int32_t seg_total_bytes;
    u_int32_t num_segs;
    u_int32_t seg_size;
    u_int32_t seg_size_log2;
    u_int32_t seg_busaddr[SWAP_MAX_SEGS];
    void *seg_cpuaddr[SWAP_MAX_SEGS];
};




typedef enum _ATH_SWAP_INFO {
    ATH_TARGET_OTP_CODE_SWAP = 0,
    ATH_TARGET_OTP_DATA_SWAP,
    ATH_TARGET_BIN_CODE_SWAP,
    ATH_TARGET_BIN_DATA_SWAP,
    ATH_TARGET_BIN_UTF_CODE_SWAP,
    ATH_TARGET_BIN_UTF_DATA_SWAP,
} ATH_SWAP_INFO;


#define SWAP_FILE_PATH                     "/lib/firmware/"
#define TARGET_CODE_SWAP_FILE_EXT_TYPE     "codeswap.bin"
#define TARGET_DATA_SWAP_FILE_EXT_TYPE     "dataswap.bin"



int
ol_swap_seg_alloc (ol_ath_soc_softc_t *soc, struct swap_seg_info **ret_seg_info, 
                                             u_int64_t **scn_cpuaddr, const char* filename, int type);
int 
ol_swap_wlan_memory_expansion(ol_ath_soc_softc_t *soc, struct swap_seg_info *seg_info,
                                                  const char*  filename, u_int32_t *target_addr);


int ol_transfer_swap_struct(ol_ath_soc_softc_t *soc, ATH_SWAP_INFO swap_info,
                                                                   char *bin_filename);


int ol_ath_code_data_swap(ol_ath_soc_softc_t *soc,const char * bin_filename, 
                                                           ATH_BIN_FILE file_type);

