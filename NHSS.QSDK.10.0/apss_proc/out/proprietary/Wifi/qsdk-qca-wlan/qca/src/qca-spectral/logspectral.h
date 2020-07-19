/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/* Application to log spectral data from 11ac chipsets */


#ifndef _LOGSPECTRAL_H
#define _LOGSPECTRAL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <sys/types.h>
#include <net/if.h>

#define WSTR(a) STR(a)
#define STR(a) #a

#define here()  printf("SPECTRAL : %s : %d\n", __func__, __LINE__)

#define LOGSPECTRAL_VERSION                    "0.1"
#define SPECTRAL_SCAN_PRIORITY_DEFAULT         (0x1)
#define SPECTRAL_SCAN_COUNT_DEFAULT            (0x0)
#define SPECTRAL_SCAN_DBM_ADJ_DEFAULT          (0x0)  
#define SPECTRAL_SCAN_PWR_FORMAT_DEFAULT       (0x1)
#define SPECTRAL_SCAN_FFT_SIZE_DEFAULT         (0x7)
#define SPECTRAL_SCAN_RSSI_RPT_MODE_DEFAULT    (0x1) 
#define SPECTRAL_SCAN_WB_RPT_MODE_DEFAULT      (0x0) 
#define SPECTRAL_SCAN_STR_BIN_THR_DEFAULT      (0x7) 
#define SPECTRAL_SCAN_NB_TONE_THR_DEFAULT      (0xc) 
#define SPECTRAL_SCAN_RSSI_THR_DEFAULT         (0x1) 
#define SPECTRAL_SCAN_RESTART_ENA_DEFAULT      (0x0) 
#define SPECTRAL_SCAN_GC_ENA_DEFAULT           (0x1) 
#define SPECTRAL_SCAN_PERIOD_DEFAULT           (250) 
#define SPECTRAL_SCAN_BIN_SCALE_DEFAULT        (0x1)
#define SPECTRAL_SCAN_RPT_MODE_DEFAULT         (0x2)
#define SPECTRAL_SCAN_CHN_MASK_DEFAULT         (0x1)
#define SPECTRAL_SCAN_INIT_DELAY_DEFAULT       (0x50)
#define SPECTRAL_SCAN_NOISE_FLOOR_REF_DEFAULT  (0xa0)
#define SPECTRAL_SCAN_ACTIVE_DEFAULT           (0x1)

#ifndef KERNEL
#define __iomem
#define atomic_t long
#endif

#ifndef NETLINK_ATHEROS 
#define NETLINK_ATHEROS 17
#endif

#define SPECTRAL_MAX_TLV_SIDE               8000
#define TLV_TAG_SPECTRAL_SUMMARY_REPORT     0xF9
#define TLV_TAG_ADC_REPORT                  0xFA
#define TLV_TAG_SEARCH_FFT_REPORT           0xFB

#define LOG_FILENAME_MAX                    255
#define PHY_DATA_LENGTH_THRESH_VALUE        0x64

typedef struct logspectral_config {
    FILE* logfile;
    FILE* logfile2;
    char logfile_name[LOG_FILENAME_MAX];
    char logfile2_name[LOG_FILENAME_MAX];
    char radio_ifname[IFNAMSIZ];
    uint8_t spectral_scan_priority;
    uint32_t spectral_scan_count;
    uint8_t spectral_scan_dBm_adj;
    uint8_t spectral_scan_pwr_format;
    uint8_t spectral_scan_fft_size;
    
    uint8_t spectral_scan_rssi_rpt_mode;
    uint8_t spectral_scan_wb_rpt_mode;
    uint8_t spectral_scan_str_bin_thr;
    uint8_t spectral_scan_nb_tone_thr;
    uint8_t spectral_scan_rssi_thr;
    uint8_t spectral_scan_restart_ena;
    uint8_t spectral_scan_gc_ena;
    uint8_t spectral_scan_period;
    uint8_t spectral_scan_bin_scale;        
    uint8_t spectral_scan_rpt_mode;       
    uint8_t spectral_scan_chn_mask;      
    uint8_t spectral_scan_init_delay;       
    int8_t spectral_scan_noise_floor_ref;


    /* Netlink sockets */
    int netlink_fd;
    struct sockaddr_nl  src_addr;

    /* Socket to read/write registers */
    int reg_fd;

    /* Dump PHY Debug memory */
    int dump_phydbg_mem;
} logspectral_config_t;

/* Extern function declarations */
extern int logspectral_config_init(logspectral_config_t *config);
extern int dev_init();
extern int write_reg(int dev,  u_int32_t address, u_int32_t param);
extern int read_reg(int dev, u_int32_t address, u_int32_t* param);

#endif /* _LOGSPECTRAL_H */
