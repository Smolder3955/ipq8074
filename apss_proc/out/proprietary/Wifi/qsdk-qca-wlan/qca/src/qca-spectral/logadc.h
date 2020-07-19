/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/* Application to log ADC data from 11ac chipsets */


#ifndef _LOGADC_H
#define _LOGADC_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <net/if.h>

#define WSTR(a) STR(a)
#define STR(a) #a

#ifndef KERNEL
#define __iomem
#define atomic_t long
#endif


#define LOGADC_VERSION                          "0.1"

#define ADC_CAPTURE_FORMAT_DEFAULT              (1)
#define ADC_CAPTURE_LENGTH_DEFAULT              (1024)
#define ADC_CAPTURE_SCALE_DEFAULT               (0x0)
#define ADC_CAPTURE_GC_ENA_DEFAULT              (0x1)
#define ADC_CAPTURE_CHN_IDX_DEFAULT                     (0x0)

#ifndef NETLINK_ATHEROS 
#define NETLINK_ATHEROS 17
#endif

#define ADC_MAX_TLV_SIZE                    8000
#define TLV_TAG_SPECTRAL_SUMMARY_REPORT     0xF9
#define TLV_TAG_ADC_REPORT                  0xFA
#define TLV_TAG_SEARCH_FFT_REPORT           0xFB

#define LOG_FILENAME_MAX                    255

#define PHY_DATA_LENGTH_THRESH_VALUE        0x64

typedef struct logadc_config {
    FILE* logfile;
    char logfile_name[LOG_FILENAME_MAX];
    char radio_ifname[IFNAMSIZ];
    
    uint8_t  adc_capture_format;
    uint16_t adc_capture_length;
    uint8_t  adc_capture_scale;
    uint8_t  adc_capture_gc_ena;
    uint8_t  adc_capture_chn_idx;
    
       /* Netlink sockets */
    int netlink_fd;
    struct sockaddr_nl  src_addr;

    /* Socket to read/write registers */
    int reg_fd;
} logadc_config_t;

/* Extern function declarations */
extern int logadc_config_init(logadc_config_t *config);

#endif /* _LOGADC_H */
