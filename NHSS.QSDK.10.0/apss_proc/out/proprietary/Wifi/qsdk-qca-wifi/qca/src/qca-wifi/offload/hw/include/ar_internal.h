/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __AR_INTERNAL_H
#define __AR_INTERNAL_H

#include "ar_ops.h"

struct ar_s {
    rx_handle_t             htt_handle;
    qdf_device_t         osdev;
    int                     target_type;
    struct ar_rx_desc_ops   *ar_rx_ops;
    struct htt_rx_callbacks *rx_callbacks;
};

#endif //__AR_INTERNAL_H
