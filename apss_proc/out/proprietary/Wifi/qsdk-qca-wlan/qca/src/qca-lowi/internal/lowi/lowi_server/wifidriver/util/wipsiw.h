#ifdef __cplusplus
extern "C" {
#endif

#ifndef __WIPS_IW_H__
#define __WIPS_IW_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        WIPS module - Wifi Scanner Interface for Positioning System

GENERAL DESCRIPTION
  This file contains the declaration and some global constants for WIPS
  module.

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/
#include <stdint.h>

#define MAX_ELEMENTS_2G_ARR 14
#define MAX_ELEMENTS_5G_ARR 56

// Structure to keep the channels supported for discovery scan
// by the kernel.
typedef struct s_ch_info {
  // Num of 2g channels supported
  uint8 num_2g_ch;
  // array of center frequencies for supported 2g channels
  int arr_2g_ch[MAX_ELEMENTS_2G_ARR];
  // Num of 5g channels supported
  uint8 num_5g_ch;
  // array of center frequencies for supported 5g channels
  int arr_5g_ch[MAX_ELEMENTS_5G_ARR];
}s_ch_info;

int Wips_nl_shutdown_communication();
int Wips_nl_init_pipe();
int Wips_nl_close_pipe();
int WipsGetSupportedChannels(s_ch_info* p_ch_info);

#endif // __WIPS_IW_H__

#ifdef __cplusplus
}
#endif
