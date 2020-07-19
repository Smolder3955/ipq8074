/**************************************************************************
 *
 * This module contains the software implementation for sha256.
 *
 * Copyright (c) 2016-2017 Qualcomm Technologies, Inc.
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All rights reserved.
 * Qualcomm Technologies, Inc. Confidential and Proprietary.
 *
 *************************************************************************/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header: //components/rel/boot.bf/3.3.1/boot_images/core/storage/tools/fh_loader/fh_loader_sha.h#3 $
  $DateTime: 2017/05/16 09:41:44 $
  $Author: pwbldsvc $

when         who   what, where, why
----------   ---   ---------------------------------------------------------
04/06/17   tv      Hawkeye header changes check-in.
2016-01-14   wek   Create. Move SHA functions from security to a new file.

===========================================================================*/


#ifndef __DEVICEPROGRAMMER_SHA_H__
#define __DEVICEPROGRAMMER_SHA_H__

#include "fh_comdef.h"

#define CONTEXT_LEFTOVER_FIELD_SIZE 64

struct __sechsh_ctx_s
{
    uint32  counter[2];
    uint32  iv[16];  // is 64 byte for SHA2-512
    uint8   leftover[CONTEXT_LEFTOVER_FIELD_SIZE];
    uint32  leftover_size;
};

void sechsharm_sha256_init(   struct __sechsh_ctx_s* );
void sechsharm_sha256_update( struct __sechsh_ctx_s*, uint8*, uint32*, uint8*, uint32 );
void sechsharm_sha256_final(  struct __sechsh_ctx_s*, uint8*, uint32*, uint8* );

#endif /* __DEVICEPROGRAMMER_SHA_H__ */
