/*==================================================================
 *
 * FILE:        deviceprogrammer_storage_device.h
 *
 * DESCRIPTION:
 *   
 *
 *        Copyright (c) 2017 Qualcomm Technologies, Inc.
 *        Copyright (c) 2008-2014, 2016 Qualcomm Technologies Incorporated.
 *        All rights reserved.
 *        Qualcomm Technologies, Inc. Confidential and Proprietary.
 *               QUALCOMM Proprietary
 *==================================================================*/

/*===================================================================
 *
 *                       EDIT HISTORY FOR FILE
 *
 *   This section contains comments describing changes made to the
 *   module. Notice that changes are listed in reverse chronological
 *   order.
 *
 *   $Header: //components/rel/boot.bf/3.3.1/boot_images/core/storage/tools/deviceprogrammer_ddr/src/emmc/deviceprogrammer_storage_device.h#3 $ 
 *   $DateTime: 2017/04/11 22:02:31 $ $Author: pwbldsvc $
 *
 * YYYY-MM-DD   who     what, where, why
 * ----------   ---     ----------------------------------------------
 * 2016-10-05   wek     Add support for a partition erase
 * 2014-03-03   dks     Added MMC FFU Support
 * 2013-06-03   ah      Added legal header
 * 2013-05-31   ab      Initial checkin
 */

#ifndef DEVICEPROGRAMMER_STORAGE_DEVICE_H
#define DEVICEPROGRAMMER_STORAGE_DEVICE_H

#include "deviceprogrammer_utils.h"
#include "sdcc_api.h"

/* A ENUM used to mark which image is currently being programmed */
typedef enum
{
  PARTITION_EMMC_USER = 0,
  PARTITION_EMMC_BOOT1,
  PARTITION_EMMC_BOOT2,
  PARTITION_EMMC_RPMB,
  PARTITION_EMMC_GPP1,
  PARTITION_EMMC_GPP2,
  PARTITION_EMMC_GPP3,
  PARTITION_EMMC_GPP4,
  PARTITION_EMMC_NONE
} open_partition_type;

typedef struct {
    boolean sector_addresses_enabled;
    struct sdcc_device *hsdev_partition_handles[PARTITION_EMMC_NONE];
    int16 drivenum;
    open_partition_type current_open_partition;
    SIZE_T sector_size;
    sdcc_emmc_gpp_enh_desc_type extras;
} storage_device_t;

extern const int BLOCK_SIZE;

boolean init_storage_device_hw(storage_device_t *storedev);
void init_storage_device_struct(storage_device_t *storedev);
char * storage_device_name (storage_device_t *storedev);
boolean storage_device_open_partition(storage_device_t *storedev, byte partition);
boolean storage_device_disk_write(storage_device_t *storedev,
                                  byte* buffer,
                                  uint32 address,
                                  uint32 length);
boolean storageDeviceClosePartition(storage_device_t *storedev);
boolean storageDeviceSetBootableStorageDrive(storage_device_t *storedev);
boolean storageDeviceCreateStorageDrives(storage_device_t *storedev,
                                         SIZE_T *drive_sector_sizes,
                                         SIZE_T array_length);
SIZE_T storageDeviceGetPartitionSizeSectors(storage_device_t *storedev);
SIZE_T storageDeviceGetLowerBoundSector(storage_device_t *storedev);
SIZE_T storageDeviceGetLowerBoundSector(storage_device_t *storedev);
boolean storageDeviceErase(storage_device_t *storedev, SIZE_T start_sector, SIZE_T num_sectors);
boolean storageDeviceFormat(storage_device_t *storedev);
boolean storageDeviceRead(storage_device_t *storedev,
                          byte* buffer,
                          SIZE_T sector_address,
                          SIZE_T sector_length);
boolean storageDeviceWrite(storage_device_t *storedev,
                           byte* buffer,
                           SIZE_T sector_address,
                           SIZE_T sector_length,
                           int *errno);
boolean storageDeviceFWUpdate(storage_device_t *storedev,
                              byte* fw_bin,
                              SIZE_T num_blocks,
                              int *errno);
boolean storageDeviceSetExtras(storage_device_t *storedev,
                            const char* attribute_name,
                            const char* attribute_value);
boolean storageDeviceExtrasCommit(storage_device_t *storedev,
                                  int *errno);
boolean storageDeviceGetStorageInfo(storage_device_t *storedev);
void storageDeviceGetExtras(storage_device_t *storedev);

#endif

