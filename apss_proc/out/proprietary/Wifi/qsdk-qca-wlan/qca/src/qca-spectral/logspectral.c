/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/* Application to log spectral data from 11ac chipsets

   IMPORTANT:
   This is only temporary code that has been quickly written to
   validate spectral functionality. This code is NOT for
   any sort of production use and is likely to be in need
   of many improvements. */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <netdb.h>
#include <errno.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "logspectral.h"
#include "if_athioctl.h"
#include "ah.h"
#include "ah_internal.h"

#if defined(CONFIG_AR900B_SUPPORT)
/* Beeliner headers */
#include "drivers/include/AR900B/hw/interface/rx_location_info.h"
#include "drivers/include/AR900B/hw/interface/rx_pkt_end.h"
#include "drivers/include/AR900B/hw/interface/rx_timing_offset.h"
#include "drivers/include/AR900B/hw/interface/rx_phy_ppdu_end.h"
#include "drivers/include/AR900B/hw/sm_reg_map.h"
#include "drivers/include/AR900B/hw/mac_rxpcu_reg.h"
#include "drivers/include/AR900B/extra/hw/map_pcu_reg.h"
#include "drivers/include/AR900B/extra/hw/apb_map.h"
#include "drivers/include/AR900B/hw/bb_reg_map.h"
#include "drivers/include/AR900B/hw/tlv/rx_ppdu_start.h"
#include "drivers/include/AR900B/hw/tlv/rx_ppdu_end.h"
#else   /* CONFIG_AR900B_SUPPORT */
/* Peregrine headers */
#include "drivers/include/AR9888/hw/mac_pcu_reg.h"
#include "drivers/include/AR9888/hw/bb_reg_map.h"
#include "drivers/include/AR9888/hw/mac_descriptors/rx_ppdu_start.h"
#include "drivers/include/AR9888/hw/mac_descriptors/rx_ppdu_end.h"
#endif  /* CONFIG_AR900B_SUPPORT */
#include "drivers/include/spectral_adc_defs.h"


#if defined(CONFIG_AR900B_SUPPORT)
static void start_ar900b_phy_debug();
static void stop_ar900b_phy_debug();
static void dump_ar900b_phy_debug_mem();
static void dw_configure_registers_ar900b(logspectral_config_t* config);
static void dump_spectral_register_params_ar900b();
static void dump_adc_samples_ar900b();
static void print_spectral_reg_info_ar900b();
#endif  /* CONFIG_AR900B_SUPPORT */

/* Enable Reg hack */
#define CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION 1
#define SPECTRAL_DATA_IN_TLV_FORMAT                 1
#define DBG_DUMP_AR900B_REGISTER_VALUES             1

/* Global config - access this directly only where required. Else pass pointer */
logspectral_config_t config;

/* device Id */
int dev;

/* Options processing */
static const char *optString = "f:g:i:a:b:c:d:D:e:j:k:l:m:n:o:p:Pq:r:s:t:u:v:xyh?";

void dump_spectral_register_params();

static const struct option longOpts[] = {
    { "file", required_argument, NULL, 'f' },
    { "file2", required_argument, NULL, 'g' },
    { "device_name", required_argument, NULL, 'D' },
    { "interface", required_argument, NULL, 'i' },
    { "spectral_scan_priority", required_argument, NULL, 'a' },
    { "spectral_scan_count", required_argument, NULL, 'b' },
    { "spectral_scan_dBm_adj", required_argument, NULL, 'c' },
    { "spectral_scan_pwr_format", required_argument, NULL, 'd' },
    { "spectral_scan_fft_size", required_argument, NULL, 'e' },
    { "spectral_scan_rssi_rpt_mode", required_argument, NULL, 'j' },
    { "spectral_scan_wb_rpt_mode", required_argument, NULL, 'k' },
    { "spectral_scan_str_bin_thr", required_argument, NULL, 'l' },
    { "spectral_scan_nb_tone_thr", required_argument, NULL, 'm' },
    { "spectral_scan_rssi_thr", required_argument, NULL, 'n' },
    { "spectral_scan_restart_ena", required_argument, NULL, 'o' },
    { "spectral_scan_gc_ena", required_argument, NULL, 'p' },
    { "Pause", no_argument, NULL, 'P' },
    { "spectral_scan_period", required_argument, NULL, 'q' },
    { "spectral_scan_bin_scale", required_argument, NULL, 'r' },
    { "spectral_scan_rpt_mode", required_argument, NULL, 's' },
    { "spectral_scan_chn_mask", required_argument, NULL, 't' },
    { "spectral_scan_init_delay", required_argument, NULL, 'u' },
    { "spectral_scan_noise_floor_ref", required_argument, NULL, 'v' },
    { "dump_reg", no_argument, NULL, 'x' },
    { "dump_mem", no_argument, NULL, 'y' },
    { NULL, no_argument, NULL, 0 },
};


/* Static functions */
static int configure_registers(logspectral_config_t *config);
static void signal_handler(int signal);
static void cleanup(logspectral_config_t *config);
static void hexdump(FILE* fp, unsigned char *buf, unsigned int len);
static int process_spectral_adc_sample(logspectral_config_t *config);
static int obtain_spectral_samples(logspectral_config_t *config);
static void display_help();
static uint32_t regread(logspectral_config_t *config, u_int32_t off);
static void regwrite(logspectral_config_t *config, uint32_t off, uint32_t value);
static int process_spectral_summary_report(logspectral_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen);
static int process_search_fft_report(logspectral_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen);
static int process_adc_report(logspectral_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen);
static int process_phy_tlv(logspectral_config_t *config, uint32_t serial_num, void* buf);
static int process_rx_ppdu_start(logspectral_config_t *config, uint32_t serial_num, struct rx_ppdu_start *ppdu_start);
static int process_rx_ppdu_end(logspectral_config_t *config, uint32_t serial_num, struct rx_ppdu_end *ppdu_end);
static int dw_configure_registers(logspectral_config_t* config);
static void print_spectral_config(logspectral_config_t *config);

#define regupdate(config, REGISTERNAME, REGISTERFIELD, value) \
{\
    uint32_t regval;\
    regval= (regread((config), PHY_##REGISTERNAME##_ADDRESS) & (~PHY_##REGISTERNAME##_##REGISTERFIELD##_MASK)) | PHY_##REGISTERNAME##_##REGISTERFIELD##_SET((value));\
    regwrite((config), PHY_##REGISTERNAME##_ADDRESS, regval);\
}

#define macregupdate(config, REGISTERNAME, REGISTERFIELD, value) \
{\
    uint32_t regval;\
    regval= (regread((config), REGISTERNAME##_ADDRESS) & (~REGISTERNAME##_##REGISTERFIELD##_MASK)) | REGISTERNAME##_##REGISTERFIELD##_SET((value));\
    regwrite((config), REGISTERNAME##_ADDRESS, regval);\
}


#if CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION
/*
 * Temp functions to use direct read/write from PCI space
 */
#define update_reg(dev, REGISTERNAME, REGISTERFIELD, value) \
{\
    uint32_t regval;\
    uint32_t temp;\
    regval= (read_reg((dev), PHY_##REGISTERNAME##_ADDRESS, &temp) & (~PHY_##REGISTERNAME##_##REGISTERFIELD##_MASK)) | PHY_##REGISTERNAME##_##REGISTERFIELD##_SET((value));\
    write_reg((dev), PHY_##REGISTERNAME##_ADDRESS, regval);\
    printf("\n");\
}

#define read_bb_reg_field(dev, REGISTERNAME, REGISTERFIELD, value) \
{\
    uint32_t regval; \
    uint32_t temp; \
    read_reg((dev), PHY_##REGISTERNAME##_ADDRESS, &temp);\
    value = PHY_##REGISTERNAME##_##REGISTERFIELD##_GET(temp);\
}

/*
 * Temp functions to use direct read/write from PCI space
 */
#define update_mac_reg(dev, REGISTERNAME, REGISTERFIELD, value) \
{\
    uint32_t regval;\
    regval= (read_reg((dev), REGISTERNAME##_ADDRESS, &regval) & (~REGISTERNAME##_##REGISTERFIELD##_MASK)) | REGISTERNAME##_##REGISTERFIELD##_SET((value));\
    write_reg((dev), REGISTERNAME##_ADDRESS, regval);\
    printf("\n");\
}

#endif  // CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION

void dump_spectral_register_params()
{
    int val;
    int reg_ss_1;
    int reg_ss_2;
    int reg_ss_3;
    int fft_ctl_1;
    int fft_ctl_2;
    int fft_ctl_4;

    // PHY_BB_SPECTRAL_SCAN_ADDRESS (0x2a228)
    reg_ss_1 = read_reg(dev, PHY_BB_SPECTRAL_SCAN_ADDRESS, &val);

    printf("Enable                = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_ENA_GET(reg_ss_1));
    printf("Active                = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_ACTIVE_GET(reg_ss_1));
    printf("FFT Size              = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_FFT_SIZE_GET(reg_ss_1));
    printf("Scan Period           = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_PERIOD_GET(reg_ss_1));
    printf("Scan Count            = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_COUNT_GET(reg_ss_1));
    printf("Priority              = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_PRIORITY_GET(reg_ss_1));
    printf("GC Enable             = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_GC_ENA_GET(reg_ss_1));
    printf("Restart Enable        = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_RESTART_ENA_GET(reg_ss_1));

    // PHY_BB_SPECTRAL_SCAN_2_ADDRESS (0x29e98)
    reg_ss_2 = read_reg(dev, PHY_BB_SPECTRAL_SCAN_2_ADDRESS, &val);

    printf("Noise Floor ref (hex) = 0x%02x\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_NOISE_FLOOR_REF_GET(reg_ss_2));
    printf("Noise Floor ref       = %d\n", (signed char)PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_NOISE_FLOOR_REF_GET(reg_ss_2));
    printf("INT Delay             = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_INIT_DELAY_GET(reg_ss_2));
    printf("NB Tone thr           = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_NB_TONE_THR_GET(reg_ss_2));
    printf("String bin thr        = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_STR_BIN_THR_GET(reg_ss_2));
    printf("WB report mode        = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_WB_RPT_MODE_GET(reg_ss_2));
    printf("RSSI report           = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_RSSI_RPT_MODE_GET(reg_ss_2));

    // PHY_BB_SPECTRAL_SCAN_3_ADDRESS (0x29e9c)
    reg_ss_3 = read_reg(dev, PHY_BB_SPECTRAL_SCAN_3_ADDRESS, &val);

    printf("RSSI Threshold (hex)  = 0x%02x\n", PHY_BB_SPECTRAL_SCAN_3_SPECTRAL_SCAN_RSSI_THR_GET(reg_ss_3));
    printf("RSSI Threshold        = %d\n", (signed char)PHY_BB_SPECTRAL_SCAN_3_SPECTRAL_SCAN_RSSI_THR_GET(reg_ss_3));

    // PHY_BB_SRCH_FFT_CTRL_1_ADDRESS (0x29e80)
    fft_ctl_1 = read_reg(dev, PHY_BB_SRCH_FFT_CTRL_1_ADDRESS, &val);

    printf("PWR format            = %d\n", PHY_BB_SRCH_FFT_CTRL_1_SPECTRAL_SCAN_PWR_FORMAT_GET(fft_ctl_1));
    printf("RPT Mode              = %d\n", PHY_BB_SRCH_FFT_CTRL_1_SPECTRAL_SCAN_RPT_MODE_GET(fft_ctl_1));
    printf("Bin Scale             = %d\n", PHY_BB_SRCH_FFT_CTRL_1_SPECTRAL_SCAN_BIN_SCALE_GET(fft_ctl_1));
    printf("dBm Adjust            = %d\n", PHY_BB_SRCH_FFT_CTRL_1_SPECTRAL_SCAN_DBM_ADJ_GET(fft_ctl_1));

    // PHY_BB_SRCH_FFT_CTRL_4_ADDRESS (0x29e8c)
    fft_ctl_4 = read_reg(dev, PHY_BB_SRCH_FFT_CTRL_4_ADDRESS, &val);
    printf("Chain Mask            = %d\n", PHY_BB_SRCH_FFT_CTRL_4_SPECTRAL_SCAN_CHN_MASK_GET(fft_ctl_4));
}



/* Function definitions */

int main(int argc, char *argv[])
{
    int opt = 0;
    int longIndex;
    int err;
    char* dev_name = NULL;
    int dump_reg = FALSE;
    int pause = FALSE;

    logspectral_config_init(&config);

    opt = getopt_long(argc, argv, optString, longOpts, &longIndex);

    while( opt != -1 ) {
        switch(opt) {
            case 'f':
                strncpy(config.logfile_name, optarg, sizeof(config.logfile_name));
                break;

            case 'g':
                strncpy(config.logfile2_name, optarg, sizeof(config.logfile2_name));
                break;

            case 'i':
                /* TODO: Validate input */
                strncpy(config.radio_ifname, optarg, sizeof(config.radio_ifname));
                break;

            case 'a':
                /* TODO: Validate input */
                config.spectral_scan_priority = atoi(optarg);
                break;

            case 'b':
                /* TODO: Validate input */
                config.spectral_scan_count = atoi(optarg);
                break;

            case 'c':
                /* TODO: Validate input */
                config.spectral_scan_dBm_adj = atoi(optarg);
                break;

            case 'd':
                /* TODO: Validate input */
                config.spectral_scan_pwr_format = atoi(optarg);
                break;

            case 'D':
                dev_name = optarg;
                break;

            case 'e':
                /* TODO: Validate input */
                config.spectral_scan_fft_size = atoi(optarg);
                break;

            case 'j':
                /* TODO: Validate input */
                config.spectral_scan_rssi_rpt_mode = atoi(optarg);
                break;

            case 'k':
                /* TODO: Validate input */
                config.spectral_scan_wb_rpt_mode = atoi(optarg);
                break;

            case 'l':
                /* TODO: Validate input */
                config.spectral_scan_str_bin_thr = atoi(optarg);
                break;

            case 'm':
                /* TODO: Validate input */
                config.spectral_scan_nb_tone_thr = atoi(optarg);
                break;

            case 'n':
                /* TODO: Validate input */
                config.spectral_scan_rssi_thr = atoi(optarg);
                break;

            case 'o':
                /* TODO: Validate input */
                config.spectral_scan_restart_ena = atoi(optarg);
                break;

            case 'p':
                /* TODO: Validate input */
                config.spectral_scan_gc_ena = atoi(optarg);
                break;

            case 'P':
                pause = TRUE;
                break;

            case 'q':
                /* TODO: Validate input */
                config.spectral_scan_period = atoi(optarg);
                break;

            case 'r':
                /* TODO: Validate input */
                config.spectral_scan_bin_scale = atoi(optarg);
                break;

            case 's':
                /* TODO: Validate input */
                config.spectral_scan_rpt_mode = atoi(optarg);
                break;

            case 't':
                /* TODO: Validate input */
                config.spectral_scan_chn_mask = atoi(optarg);
                break;

            case 'u':
                /* TODO: Validate input */
                config.spectral_scan_init_delay = atoi(optarg);
                break;

            case 'v':
                /* TODO: Validate input */
                config.spectral_scan_noise_floor_ref = atoi(optarg);
                break;

            case 'x':
                dump_reg = TRUE;
                break;

            case 'y':
                config.dump_phydbg_mem = TRUE;
                break;

            case 'h':
            case '?':
                display_help();
                return 0;

            default:
                fprintf(stderr, "Unrecognized option\n");
                display_help();
                return -1;
        }

        opt = getopt_long(argc, argv, optString, longOpts, &longIndex);
    }

    signal(SIGINT, signal_handler);
    signal(SIGCHLD, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);

#if defined(CONFIG_AR900B_SUPPORT)
    printf("--- Configured for Beeliner (AR900B) ---\n");
#endif  /* CONFIG_AR900B_SUPPORT */

    if ((config.logfile = fopen(config.logfile_name, "a")) == NULL) {
        perror("fopen");
        return -1;
    }

    /* init netlink connection to driver */
    config.netlink_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ATHEROS);

    /* validate */
    if (config.netlink_fd < 0) {
        perror("netlink error");
        cleanup(&config);
        return -1;
    }
    /* init netlink socket */
    memset(&config.src_addr, 0, sizeof(config.src_addr));
    config.src_addr.nl_family  = PF_NETLINK;
    config.src_addr.nl_pid     = getpid();
    config.src_addr.nl_groups  = 1;

    if (bind(config.netlink_fd, (struct sockaddr*)&config.src_addr, sizeof(config.src_addr)) < 0) {
        perror("netlink bind error\n");
        cleanup(&config);
        return -1;
    }

    config.reg_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (config.reg_fd < 0) {
        perror("netlink bind error\n");
        cleanup(&config);
        return -1;
    }

#if CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION
    /*
     * Temp hack to get logspectral working on QCA Main driver
     * Open the device, conigure and wait
     */
    dev = dev_init(dev_name);

    /* check if reg dump is required */
    if (dump_reg) {
#if defined(CONFIG_AR900B_SUPPORT)
       dump_spectral_register_params_ar900b();
#else   /* CONFIG_AR900B_SUPPORT */
       dump_spectral_register_params();
#endif  /* CONFIG_AR900B_SUPPORT */
        return 0;
    }
#if defined(CONFIG_AR900B_SUPPORT)
    dw_configure_registers_ar900b(&config);
#else   /* CONFIG_AR900B_SUPPORT */
    dw_configure_registers(&config);
#endif  /* CONFIG_AR900B_SUPPORT */

#else  /* CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION */
    configure_registers(&config);
#endif  /* CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION */


    /* Tool hack: Helps in just configuration */
    if (pause) {
        while(1);
    }

    //Carry out the main processing
    return obtain_spectral_samples(&config);
}


#if CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION
/*
 * Configure registers, but directly writing/reading
 * via PCI address space
 */

static int dw_configure_registers(logspectral_config_t* config)
{
     /* Editing note: Keep column alignment to help easy regex based changes */

    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ENA,            0x0);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ACTIVE,         0x0);

    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ENA,            0x1);

    update_mac_reg(dev, MAC_PCU_RX_FILTER,  PHY_DATA,                  0x1);
    update_mac_reg(dev, MAC_PCU_RX_FILTER,  PROMISCUOUS,               0x0);


    write_reg(dev,  PHY_BB_WATCHDOG_CTRL_1_ADDRESS, 0x00080013);
    write_reg(dev,  PHY_BB_WATCHDOG_CTRL_2_ADDRESS, 0x700);
    /* Program the PHY ERROR mask, remove magic number */
    write_reg(dev, 0x28338, 0x50);

    update_reg(dev, BB_EXTENSION_RADAR, RADAR_LB_DC_CAP, 60);

    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_FFT_SIZE,       config->spectral_scan_fft_size);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_COUNT,          config->spectral_scan_count);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_PERIOD,         config->spectral_scan_period);

    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_PRIORITY,       config->spectral_scan_priority);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_RESTART_ENA,    config->spectral_scan_restart_ena);

    update_reg(dev, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_DBM_ADJ,        config->spectral_scan_dBm_adj);
    update_reg(dev, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_RPT_MODE,       config->spectral_scan_rpt_mode);
    update_reg(dev, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_PWR_FORMAT,     config->spectral_scan_pwr_format);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_WB_RPT_MODE,    config->spectral_scan_wb_rpt_mode);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_RSSI_RPT_MODE,  config->spectral_scan_rssi_rpt_mode);
    update_reg(dev, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_BIN_SCALE,      config->spectral_scan_bin_scale);
    update_reg(dev, BB_SRCH_FFT_CTRL_4, SPECTRAL_SCAN_CHN_MASK,       config->spectral_scan_chn_mask);

    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_STR_BIN_THR,    config->spectral_scan_str_bin_thr);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_NB_TONE_THR,    config->spectral_scan_nb_tone_thr);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_INIT_DELAY,     config->spectral_scan_init_delay);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_NOISE_FLOOR_REF,config->spectral_scan_noise_floor_ref); //default=0xa0 #0x96 for emulation due to the scaled down BW and the missing analog filter

    update_reg(dev, BB_SPECTRAL_SCAN_3, SPECTRAL_SCAN_RSSI_THR,       config->spectral_scan_rssi_thr);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_GC_ENA,         config->spectral_scan_gc_ena);

    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ACTIVE,         0x1);

    //print_spectral_config(config);
    dump_spectral_register_params();
    return 0;
}

#endif  // CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION

static int configure_registers(logspectral_config_t *config)
{

    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ENA,            0x0);
    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ACTIVE,         0x0);

    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ENA,            0x1);

    //regupdate(config, BB_MODES_SELECT,    DYN_OFDM_CCK_MODE,            0x0);
    macregupdate(config, MAC_PCU_RX_FILTER,  PHY_DATA,                  0x1);
    macregupdate(config, MAC_PCU_RX_FILTER,  PROMISCUOUS,               0x0);

    //regwrite(config,  BB_WATCHDOG_CTRL_1_ADDRESS, 0xfffffffd);
#if 0
    regwrite(config,  BB_WATCHDOG_CTRL_1_ADDRESS, 0x00080013);
    regwrite(config,  BB_WATCHDOG_CTRL_2_ADDRESS, 0x700);
#endif
    regwrite(config,  PHY_BB_WATCHDOG_CTRL_1_ADDRESS, 0x00080013);
    regwrite(config,  PHY_BB_WATCHDOG_CTRL_2_ADDRESS, 0x700);

    regupdate(config, BB_EXTENSION_RADAR, RADAR_LB_DC_CAP, 60);

    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_FFT_SIZE,       config->spectral_scan_fft_size);
    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_COUNT,          config->spectral_scan_count);
    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_PERIOD,         config->spectral_scan_period);

    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_PRIORITY,       config->spectral_scan_priority);
    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_RESTART_ENA,    config->spectral_scan_restart_ena);

    regupdate(config, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_DBM_ADJ,        config->spectral_scan_dBm_adj);
    regupdate(config, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_RPT_MODE,       config->spectral_scan_rpt_mode);
    regupdate(config, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_PWR_FORMAT,     config->spectral_scan_pwr_format);
    regupdate(config, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_WB_RPT_MODE,    config->spectral_scan_wb_rpt_mode);
    regupdate(config, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_RSSI_RPT_MODE,  config->spectral_scan_rssi_rpt_mode);
    regupdate(config, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_BIN_SCALE,      config->spectral_scan_bin_scale);
    regupdate(config, BB_SRCH_FFT_CTRL_4, SPECTRAL_SCAN_CHN_MASK,       config->spectral_scan_chn_mask);

    regupdate(config, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_STR_BIN_THR,    config->spectral_scan_str_bin_thr);
    regupdate(config, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_NB_TONE_THR,    config->spectral_scan_nb_tone_thr);
    regupdate(config, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_INIT_DELAY,     config->spectral_scan_init_delay);
    regupdate(config, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_NOISE_FLOOR_REF,config->spectral_scan_noise_floor_ref); //default=0xa0 #0x96 for emulation due to the scaled down BW and the missing analog filter

    regupdate(config, BB_SPECTRAL_SCAN_3, SPECTRAL_SCAN_RSSI_THR,       config->spectral_scan_rssi_thr);
    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_GC_ENA,         config->spectral_scan_gc_ena);

    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ACTIVE,         0x1);
}

static void signal_handler(int signal)
{
    switch (signal) {
        case SIGHUP:
        case SIGTERM:
        case SIGCHLD:
        case SIGINT:
            cleanup(&config);
            break;
        default:
            printf("%s:Unknown signal!", __func__);
            break;
    }
}

static void cleanup(logspectral_config_t *config)
{
#if CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION
    update_reg(dev, BB_SPECTRAL_SCAN, SPECTRAL_SCAN_ACTIVE, 0x0);
    usleep(20);
    update_reg(dev, BB_SPECTRAL_SCAN, SPECTRAL_SCAN_ENA, 0x0);
#else
    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ACTIVE,         0x0);
    usleep(20);
    regupdate(config, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ENA,            0x0);
#endif

    if (config->logfile != NULL) {
        fclose(config->logfile);
    }

    if (config->logfile2 != NULL) {
        fclose(config->logfile2);
    }

    if (config->netlink_fd) {
        close(config->netlink_fd);
    }

    if (config->reg_fd) {
        close(config->reg_fd);
    }

#if defined(CONFIG_AR900B_SUPPORT)
    dump_spectral_register_params_ar900b();
#else   /* CONFIG_AR900B_SUPPORT */
    //print_spectral_config(config);
    dump_spectral_register_params();
#endif  /* CONFIG_AR900B_SUPPORT */

#if defined(CONFIG_AR900B_SUPPORT)
    if (config->dump_phydbg_mem) {
        config->dump_phydbg_mem = 0;
        stop_ar900b_phy_debug();
        dump_ar900b_phy_debug_mem();
        dump_adc_samples_ar900b();
    }
#endif  /* CONFIG_AR900B_SUPPORT */

    exit(0);
}

int logspectral_config_init(logspectral_config_t *config)
{
    if (NULL == config) {
        return -1;
    }

    config->logfile = NULL;
    config->logfile2 = NULL;
    strncpy(config->logfile_name, "/tmp/SS_data.txt", sizeof (config->logfile_name));
    strncpy(config->logfile2_name, "/tmp/SS_data2.txt", sizeof (config->logfile2_name));
    strncpy(config->radio_ifname, "wifi0", sizeof(config->radio_ifname));
    config->spectral_scan_priority =      SPECTRAL_SCAN_PRIORITY_DEFAULT;
    config->spectral_scan_count =         SPECTRAL_SCAN_COUNT_DEFAULT;
    config->spectral_scan_dBm_adj =       SPECTRAL_SCAN_DBM_ADJ_DEFAULT;
    config->spectral_scan_pwr_format =    SPECTRAL_SCAN_PWR_FORMAT_DEFAULT;
    config->spectral_scan_fft_size =      SPECTRAL_SCAN_FFT_SIZE_DEFAULT;
    config->spectral_scan_rssi_rpt_mode = SPECTRAL_SCAN_RSSI_RPT_MODE_DEFAULT;
    config->spectral_scan_wb_rpt_mode =   SPECTRAL_SCAN_WB_RPT_MODE_DEFAULT;
    config->spectral_scan_str_bin_thr =   SPECTRAL_SCAN_STR_BIN_THR_DEFAULT;
    config->spectral_scan_nb_tone_thr =   SPECTRAL_SCAN_NB_TONE_THR_DEFAULT;
    config->spectral_scan_rssi_thr =      SPECTRAL_SCAN_RSSI_THR_DEFAULT;
    config->spectral_scan_restart_ena =   SPECTRAL_SCAN_RESTART_ENA_DEFAULT;
    config->spectral_scan_gc_ena =        SPECTRAL_SCAN_GC_ENA_DEFAULT;
    config->spectral_scan_period =        SPECTRAL_SCAN_PERIOD_DEFAULT;
    config->spectral_scan_bin_scale =     SPECTRAL_SCAN_BIN_SCALE_DEFAULT;
    config->spectral_scan_rpt_mode =      SPECTRAL_SCAN_RPT_MODE_DEFAULT;
    config->spectral_scan_chn_mask =      SPECTRAL_SCAN_CHN_MASK_DEFAULT;
    config->spectral_scan_init_delay =    SPECTRAL_SCAN_INIT_DELAY_DEFAULT;
    config->spectral_scan_noise_floor_ref = SPECTRAL_SCAN_NOISE_FLOOR_REF_DEFAULT;

    return 0;
}

static void hexdump(FILE *fp, unsigned char *buf, unsigned int len)
{
    while (len--) {
        fprintf(fp, "%02x", *buf++);
    }

    fprintf(fp, "\n");
}

static int process_spectral_adc_sample(logspectral_config_t *config)
{
    struct nlmsghdr  *nlh = NULL;
    /* XXX Change this approach if we go multi-threaded!! */
    static char buf[NLMSG_SPACE(SPECTRAL_MAX_TLV_SIDE)];
    int bytes_read = 0;
    uint8_t *tempbuf;
    spectral_adc_log_desc *desc;

    nlh = (struct nlmsghdr *)buf;

    memset(nlh, 0, NLMSG_SPACE(sizeof(SPECTRAL_MAX_TLV_SIDE)));
    nlh->nlmsg_len  = NLMSG_SPACE(sizeof(SPECTRAL_MAX_TLV_SIDE));
    nlh->nlmsg_pid  = getpid();
    nlh->nlmsg_flags = 0;

    bytes_read = recvfrom(config->netlink_fd,
                      buf,
                      sizeof(buf),
                      MSG_WAITALL,
                      NULL,
                      NULL);

    tempbuf = (uint8_t *)NLMSG_DATA(nlh);

#if SPECTRAL_DATA_IN_TLV_FORMAT
    /*
     * If the data is in TLV format, use the TLV processing
     * functions else use old PPDU format processing 
     */

    spectral_process_phyerr_data(tempbuf);
#else   /* SPECTRAL_DATA_IN_TLV_FORMAT */

    desc = (spectral_adc_log_desc *)tempbuf;

    switch(desc->type)
    {
        case SPECTRAL_ADC_LOG_TYPE_RX_PPDU_START:
            return process_rx_ppdu_start(config, desc->serial_num, (struct rx_ppdu_start*)(tempbuf + sizeof(spectral_adc_log_desc)));
            break;
        case SPECTRAL_ADC_LOG_TYPE_RX_PPDU_END:
            return process_rx_ppdu_end(config, desc->serial_num, (struct rx_ppdu_end*)(tempbuf + sizeof(spectral_adc_log_desc)));
            break;
        case SPECTRAL_ADC_LOG_TYPE_TLV:
            return process_phy_tlv(config, desc->serial_num, (void*)(tempbuf + sizeof(spectral_adc_log_desc)));
            break;
        default:
            fprintf(stderr, "Unknown log description type!\n");
            return -1;
    }
#endif  /* SPECTRAL_ADC_LOG_TYPE_TLV */
    return 0;
}

static int process_rx_ppdu_start(logspectral_config_t *config, uint32_t serial_num, struct rx_ppdu_start *ppdu_start)
{
    printf("\n===Rx PPDU Start. Sr no.%u ===\n", serial_num);
#if defined(CONFIG_AR900B_SUPPORT)
    printf("\nrssi_pri_chain0 = %d, rssi_sec20_chain0 = %d, rssi_sec40_chain0 = %d, rssi_comb=%d\n", ppdu_start->rssi_pri_chain0, ppdu_start->rssi_sec20_chain0, ppdu_start->rssi_sec40_chain0, ppdu_start->rssi_comb);
#else   /* CONFIG_AR900B_SUPPORT */
    printf("\nrssi_chain0_pri20 = %d, rssi_chain0_sec20 = %d, rssi_chain0_sec40 = %d, rssi_comb=%d\n", ppdu_start->rssi_chain0_pri20, ppdu_start->rssi_chain0_sec20, ppdu_start->rssi_chain0_sec40, ppdu_start->rssi_comb);
#endif  /* CONFIG_AR900B_SUPPORT */
}

static int process_rx_ppdu_end(logspectral_config_t *config, uint32_t serial_num, struct rx_ppdu_end *ppdu_end)
{
    static uint32_t last_ts = 0;

    printf("\n===Rx PPDU End. Sr no.%u ===\n", serial_num);
#if defined(CONFIG_AR900B_SUPPORT)
    printf("\nTS = %u, BB Len = %d, Delta time = %u\n", ppdu_end->tsf_timestamp, ppdu_end->bb_length, ppdu_end->tsf_timestamp - last_ts);
#else   /* CONFIG_AR900B_SUPPORT */
    printf("\nTS = %u, BB Len = %d, Error code = %d, Delta time = %u\n", ppdu_end->tsf_timestamp, ppdu_end->bb_length, ppdu_end->phy_err_code, ppdu_end->tsf_timestamp - last_ts);
#endif  /* CONFIG_AR900B_SUPPORT */
    last_ts = ppdu_end->tsf_timestamp;
}


static int process_phy_tlv(logspectral_config_t *config, uint32_t serial_num, void* buf)
{
    uint8_t *tlv;
    uint32_t *tlvheader;
    uint16_t tlvlen;
    uint8_t tlvtag;
    uint8_t signature;

    tlv = (uint8_t *)buf;

    tlvheader = (uint32_t *) tlv;
    signature = (tlvheader[0] >> 24) & 0xff;
    tlvlen = tlvheader[0] & 0x0000ffff;
    tlvtag = (tlvheader[0] >> 16) & 0xff;

    if (signature != 0xbb) {
        fprintf(stderr, "Invalid signature 0x%x! Hexdump follows\n", signature);
        hexdump(stderr, tlv, tlvlen + 4);
        return -1;
    }

    switch(tlvtag)
    {
        case TLV_TAG_SPECTRAL_SUMMARY_REPORT:
            process_spectral_summary_report(config, serial_num, tlv, tlvlen);
            break;
        case TLV_TAG_SEARCH_FFT_REPORT:
            process_search_fft_report(config, serial_num, tlv, tlvlen);
            break;
        case TLV_TAG_ADC_REPORT:
            process_adc_report(config, serial_num, tlv, tlvlen);
            break;
        default:
            fprintf(stderr, "Unknown TLV Tag ID 0x%x! Hexdump follows\n", tlvtag);
            hexdump(stderr, tlv, tlvlen + 4);
            return -1;
    }

    return 0;
}

static int obtain_spectral_samples(logspectral_config_t *config)
{
    int fdmax;
    int fd;
    fd_set  master;
    fd_set  read_fds;

    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    /* Actually, there is no need for the select call right now. But we put this in
       for extensibility */
    FD_SET(config->netlink_fd, &master);
    fdmax = config->netlink_fd;

    for (;;) {

        read_fds = master;

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            continue;
        }

        for (fd = 0; fd <= fdmax; fd++) {
            if (FD_ISSET(fd, &read_fds)) {
                if (fd ==  config->netlink_fd) {
#if 0
                        if (process_spectral_adc_sample(config) < 0) {
                            cleanup(config);
                            return -1;
                        }
#endif
                        process_spectral_adc_sample(config);
                }
             }
        }
    }
}

void read_agc()
{
    int val;
    /* TODO : Change the magic number */
    printf("\nagc  = 0x%x\n", read_reg(dev, 0x29e00, &val));
}

void read_cca()
{
    int val;
    /* TODO : Change the magic number */
    printf("mincca  0   = 0x%x\n", read_reg(dev, 0x29e1c, &val));
    printf("mincca  1   = 0x%x\n", read_reg(dev, 0x2ae1c, &val));
    printf("mincca  2   = 0x%x\n", read_reg(dev, 0x2be1c, &val));

}
void read_bb_spectral_scan_1()
{
    int val;
    /* TODO : Change the magic number */
    printf("bb spectral 1  = 0x%x\n", read_reg(dev, 0x2a228, &val));
}

void read_bb_spectral_scan_2()
{
    int val;
    /* TODO : Change the magic number */
    printf("bb spectral 2  = 0x%x\n", read_reg(dev, 0x29e98, &val));
}
void read_bb_spectral_scan_3()
{
    int val;
    /* TODO : Change the magic number */
    printf("bb spectral 3  = 0x%x\n", read_reg(dev, 0x29e9c, &val));
}


void print_spectral_config(logspectral_config_t *config)
{
    if (NULL == config) {
        return;
    }

    printf("----------------- Current Spectral Configuration--------------------\n");
    printf("SPECTRAL_SCAN_PRIORITY_DEFAULT          = %d\n", (u_int32_t)config->spectral_scan_priority);
    printf("SPECTRAL_SCAN_COUNT_DEFAULT             = %d\n", (u_int32_t)config->spectral_scan_count);
    printf("SPECTRAL_SCAN_DBM_ADJ_DEFAULT           = %d\n", (u_int32_t)config->spectral_scan_dBm_adj);
    printf("SPECTRAL_SCAN_PWR_FORMAT_DEFAULT        = %d\n", (u_int32_t)config->spectral_scan_pwr_format);
    printf("SPECTRAL_SCAN_FFT_SIZE_DEFAULT          = %d\n", (u_int32_t)config->spectral_scan_fft_size);
    printf("SPECTRAL_SCAN_RSSI_RPT_MODE_DEFAULT     = %d\n", (u_int32_t)config->spectral_scan_rssi_rpt_mode);
    printf("SPECTRAL_SCAN_WB_RPT_MODE_DEFAULT       = %d\n", (u_int32_t)config->spectral_scan_wb_rpt_mode);
    printf("SPECTRAL_SCAN_STR_BIN_THR_DEFAULT       = %d\n", (u_int32_t)config->spectral_scan_str_bin_thr);
    printf("SPECTRAL_SCAN_NB_TONE_THR_DEFAULT       = %d\n", (u_int32_t)config->spectral_scan_nb_tone_thr);
    printf("SPECTRAL_SCAN_RSSI_THR_DEFAULT          = %d\n", (u_int32_t)config->spectral_scan_rssi_thr);
    printf("SPECTRAL_SCAN_RESTART_ENA_DEFAULT       = %d\n", (u_int32_t)config->spectral_scan_restart_ena);
    printf("SPECTRAL_SCAN_GC_ENA_DEFAULT            = %d\n", (u_int32_t)config->spectral_scan_gc_ena);
    printf("SPECTRAL_SCAN_PERIOD_DEFAULT            = %d\n", (u_int32_t)config->spectral_scan_period);
    printf("SPECTRAL_SCAN_BIN_SCALE_DEFAULT         = %d\n", (u_int32_t)config->spectral_scan_bin_scale);
    printf("SPECTRAL_SCAN_RPT_MODE_DEFAULT          = %d\n", (u_int32_t)config->spectral_scan_rpt_mode);
    printf("SPECTRAL_SCAN_CHN_MASK_DEFAULT          = %d\n", (u_int32_t)config->spectral_scan_chn_mask);
    printf("SPECTRAL_SCAN_INIT_DELAY_DEFAULT        = %d\n", (u_int32_t)config->spectral_scan_init_delay);
    printf("SPECTRAL_SCAN_NOISE_FLOOR_REF_DEFAULT   = %d\n", (u_int32_t)config->spectral_scan_noise_floor_ref);

    //read_agc();
    //read_cca();
    read_bb_spectral_scan_1();
    read_bb_spectral_scan_2();
    read_bb_spectral_scan_3();
    printf("----------------- Current Spectral Configuration--------------------\n");


    return ;
}

static void display_help()
{
    printf("\nlogspectral v"LOGSPECTRAL_VERSION": Log spectral data received from 11ac chipsets for debug purposes.\n"
           "\n"
           "Usage:\n"
           "\n"
           "logspectral [OPTIONS]\n"
           "\n"
           "OPTIONS:\n"
           "\n"
           "-D, --device_name\n"
           "    Name of device\n"
           "\n"
           "-f, --file\n"
           "    Name of file to log spectral FFT data to. Default: /tmp/SS_data.txt\n"
           "-g, --file2\n"
           "    Name of secondary file to log spectral FFT data to for spectral_scan_pwr_format=1. Default: /tmp/SS_data2.txt\n"
           "\n"
           "-i, --interface\n"
           "    Name of radio interface. Default: wifi0\n"
           "\n"
           "--spectral_scan_priority\n"
           "    Value of spectral_scan_priority. Default: "WSTR(SPECTRAL_SCAN_PRIORITY_DEFAULT)"\n"
           "\n"
           "--spectral_scan_count. Default: "WSTR(SPECTRAL_SCAN_COUNT_DEFAULT)"\n"
           "    Value of spectral_scan_count.\n"
           "\n"
           "--spectral_scan_dBm_adj. Default: "WSTR(SPECTRAL_SCAN_DBM_ADJ_DEFAULT)"\n"
           "    Value of spectral_scan_dBm_adj.\n"
           "\n"
           "--spectral_scan_pwr_format. Default: "WSTR(SPECTRAL_SCAN_PWR_FORMAT_DEFAULT)"\n"
           "    Value of spectral_scan_pwr_format.\n"
           "\n"
           "--spectral_scan_fft_size. Default: "WSTR(SPECTRAL_SCAN_FFT_SIZE_DEFAULT)"\n"
           "    Value of spectral_scan_fft_size.\n"
           "\n"
           "--spectral_scan_rssi_rpt_mode. Default: "WSTR(SPECTRAL_SCAN_RSSI_RPT_MODE_DEFAULT)"\n"
           "    Value of spectral_scan_rssi_rpt_mode.\n"
           "\n"
           "--spectral_scan_wb_rpt_mode. Default: "WSTR(SPECTRAL_SCAN_WB_RPT_MODE_DEFAULT)"\n"
           "    Value of spectral_scan_wb_rpt_mode.\n"
           "\n"
           "--spectral_scan_str_bin_thr. Default: "WSTR(SPECTRAL_SCAN_STR_BIN_THR_DEFAULT)"\n"
           "    Value of spectral_scan_str_bin_thr.\n"
           "\n"
           "--spectral_scan_nb_tone_thr. Default: "WSTR(SPECTRAL_SCAN_NB_TONE_THR_DEFAULT)"\n"
           "    Value of spectral_scan_nb_tone_thr.\n"
           "\n"
           "--spectral_scan_rssi_thr. Default: "WSTR(SPECTRAL_SCAN_RSSI_THR_DEFAULT)"\n"
           "    Value of spectral_scan_rssi_thr.\n"
           "\n"
           "--spectral_scan_restart_ena. Default: "WSTR(SPECTRAL_SCAN_RESTART_ENA_DEFAULT)"\n"
           "    Value of spectral_scan_restart_ena.\n"
           "\n"
           "--spectral_scan_gc_ena. Default: "WSTR(SPECTRAL_SCAN_GC_ENA_DEFAULT)"\n"
           "    Value of spectral_scan_gc_ena.\n"
           "\n"
           "--spectral_scan_period. Default: "WSTR(SPECTRAL_SCAN_PERIOD_DEFAULT)"\n"
           "    Value of spectral_scan_period.\n"
           "\n"
           "--spectral_scan_bin_scale. Default: "WSTR(SPECTRAL_SCAN_BIN_SCALE_DEFAULT)"\n"
           "    Value of spectral_scan_bin_scale.\n"
           "\n"
           "--spectral_scan_rpt_mode. Default: "WSTR(SPECTRAL_SCAN_RPT_MODE_DEFAULT)"\n"
           "    Value of spectral_scan_rpt_mode.\n"
           "\n"
           "--spectral_scan_chn_mask. Default: "WSTR(SPECTRAL_SCAN_CHN_MASK_DEFAULT)"\n"
           "    Value of spectral_scan_chn_mask.\n"
           "\n"
           "--spectral_scan_init_delay. Default: "WSTR(SPECTRAL_SCAN_INIT_DELAY_DEFAULT)"\n"
           "    Value of spectral_scan_init_delay.\n"
           "\n"
           "--spectral_scan_noise_floor_ref. Default: "WSTR(SPECTRAL_SCAN_NOISE_FLOOR_REF_DEFAULT)"\n"
           "    Value of spectral_scan_noise_floor_ref. Note that this is a signed 8-bit value.\n"
           "\n"
           "OTHER OPTIONS TO BE ADDED\n"
           "\n");
}

static uint32_t regread(logspectral_config_t *config, u_int32_t off)
{
    u_int32_t regdata;
    struct  ath_diag atd;

    strncpy(atd.ad_name, config->radio_ifname, sizeof (atd.ad_name));
    atd.ad_id = HAL_DIAG_REGREAD | ATH_DIAG_IN | ATH_DIAG_DYN;
    atd.ad_in_size = sizeof(off);
    atd.ad_in_data = (caddr_t) &off;
    atd.ad_out_size = sizeof(regdata);
    atd.ad_out_data = (caddr_t) &regdata;
    if (ioctl(config->reg_fd, SIOCGATHDIAG, &atd) < 0)
        err(1, "%s", atd.ad_name);

    return regdata;
}

static void regwrite(logspectral_config_t *config, uint32_t off, uint32_t value)
{
    HAL_DIAG_REGVAL regval;
    struct  ath_diag atd;

    regval.offset = (u_int) off ;
    regval.val = (u_int32_t) value;

    strncpy(atd.ad_name, config->radio_ifname, sizeof (atd.ad_name));
    atd.ad_id = HAL_DIAG_REGWRITE | ATH_DIAG_IN;
    atd.ad_in_size = sizeof(regval);
    atd.ad_in_data = (caddr_t) &regval;
    atd.ad_out_size = 0;
    atd.ad_out_data = NULL;
    if (ioctl(config->reg_fd, SIOCGATHDIAG, &atd) < 0)
        err(1, "%s", atd.ad_name);
}

static int process_spectral_summary_report(logspectral_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen)
{
    /* For simplicity, everything is defined as uint32_t (except one). Proper code will later use the right sizes. */

    /* For easy comparision between MDK team and OS team, the MDK script variable names have been used */

    uint32_t agc_mb_gain;
    uint32_t sscan_gidx;
    uint32_t agc_total_gain;
    uint32_t recent_rfsat;
    uint32_t ob_flag;
    uint32_t nb_mask;
    uint32_t peak_mag;
    int16_t peak_inx;

    uint32_t ss_summary_A;
    uint32_t ss_summary_B;
    uint32_t *pss_summary_A;
    uint32_t *pss_summary_B;

    printf("\n===Spectral Summary Report. Sr no. %u===\n", serial_num);

    if (tlvlen != 8) {
        fprintf(stderr, "Unexpected TLV length %d for Spectral Summary Report! Hexdump follows\n", tlvlen);
        hexdump(stderr, tlv, tlvlen + 4);
        return -1;
    }

    pss_summary_A = (uint32_t*)(tlv + 4);
    pss_summary_B = (uint32_t*)(tlv + 8);
    ss_summary_A = *pss_summary_A;
    ss_summary_B = *pss_summary_B;

    nb_mask = ((ss_summary_B >> 22) & 0xff);
    ob_flag = ((ss_summary_B >> 30) & 0x1);
    peak_inx = (ss_summary_B  & 0xfff);

    if (peak_inx > 2047) {
        peak_inx = peak_inx - 4096;
    }

    peak_mag = ((ss_summary_B >> 12) & 0x3ff);
    agc_mb_gain = ((ss_summary_A >> 24)& 0x7f);
    agc_total_gain = (ss_summary_A  & 0x3ff);
    sscan_gidx = ((ss_summary_A >> 16) & 0xff);
    recent_rfsat = ((ss_summary_B >> 31) & 0x1);

    printf("nb_mask=0x%.2x, ob_flag=%d, peak_index=%d, peak_mag=%d, agc_mb_gain=%d, agc_total_gain=%d, sscan_gidx=%d, recent_rfsat=%d\n", nb_mask, ob_flag, peak_inx, peak_mag, agc_mb_gain, agc_total_gain, sscan_gidx, recent_rfsat);
}

static int process_search_fft_report(logspectral_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen)
{
    int i;
    uint8_t fft_mag;
    int8_t signed_fft_mag;

    /* For simplicity, everything is defined as uint32_t (except one). Proper code will later use the right sizes. */
    /* For easy comparision between MDK team and OS team, the MDK script variable names have been used */
    uint32_t relpwr_db;
    uint32_t num_str_bins_ib;
    uint32_t base_pwr;
    uint32_t total_gain_info;

    uint32_t fft_chn_idx;
    int16_t peak_inx;
    uint32_t avgpwr_db;
    uint32_t peak_mag;

    uint32_t fft_summary_A;
    uint32_t fft_summary_B;
    uint32_t *pfft_summary_A;
    uint32_t *pfft_summary_B;

    printf("\n===Search FFT Report. Sr no.%u ===\n", serial_num);

    /* Relook this */
    if (tlvlen < 8) {
        fprintf(stderr, "Unexpected TLV length %d for Spectral Summary Report! Hexdump follows\n", tlvlen);
        hexdump(stderr, tlv, tlvlen + 4);
        return -1;
    }

    if ((config->logfile2 = fopen(config->logfile2_name, "w")) == NULL) {
        perror("fopen");
        return -1;
    }
#if defined(CONFIG_AR900B_SUPPORT)
    /* RXPCU_FRAME_CNT_ADDRESS is unresolved for AR900B */
    printf("MAC_PCU_RX_FRAME_CNT=%u\n", regread(config, MAC_PCU_RX_FRAME_CNT_ADDRESS));
    printf("MAC_PCU_TX_FRAME_CNT=%u\n", regread(config, MAC_PCU_TX_FRAME_CNT_ADDRESS));
#else   /* CONFIG_AR900B_SUPPORT */

    pfft_summary_A = (uint32_t*)(tlv + 4);
    pfft_summary_B = (uint32_t*)(tlv + 8);
#endif  /* CONFIG_AR900B_SUPPORT */
    fft_summary_A = *pfft_summary_A;
    fft_summary_B = *pfft_summary_B;

    relpwr_db= ((fft_summary_B >>26) & 0x3f);
    num_str_bins_ib=fft_summary_B & 0xff;
    base_pwr = ((fft_summary_A >> 14) & 0x1ff);
    total_gain_info = ((fft_summary_A >> 23) & 0x1ff);

    fft_chn_idx= ((fft_summary_A >>12) & 0x3);
    peak_inx=fft_summary_A & 0xfff;

    if (peak_inx > 2047) {
        peak_inx = peak_inx - 4096;
    }

    avgpwr_db = ((fft_summary_B >> 18) & 0xff);
    peak_mag = ((fft_summary_B >> 8) & 0x3ff);

    printf("Base Power= 0x%x, Total Gain= %d, relpwr_db=%d, num_str_bins_ib=%d fft_chn_idx=%d peak_inx=%d avgpwr_db=%d peak_mag=%d\n", base_pwr, total_gain_info, relpwr_db, num_str_bins_ib, fft_chn_idx, peak_inx, avgpwr_db, peak_mag);

#if defined(CONFIG_AR900B_SUPPORT)
    /* MAC_PCU_RX_CLEAR_CNT_ADDRESS is unresolved for AR900B */
    printf("MAC_PCU_RX_FRAME_CNT=%u\n", regread(config, MAC_PCU_RX_FRAME_CNT_ADDRESS));
    printf("MAC_PCU_TX_FRAME_CNT=%u\n", regread(config, MAC_PCU_TX_FRAME_CNT_ADDRESS));
#else   /* CONFIG_AR900B_SUPPORT */
    printf("MAC_PCU_RX_CLEAR_CNT=%u\n", regread(config, MAC_PCU_RX_CLEAR_CNT_ADDRESS));
    printf("MAC_PCU_RX_FRAME_CNT=%u\n", regread(config, MAC_PCU_RX_FRAME_CNT_ADDRESS));
    printf("MAC_PCU_TX_FRAME_CNT=%u\n", regread(config, MAC_PCU_TX_FRAME_CNT_ADDRESS));
#endif  /* CONFIG_AR900B_SUPPORT */

    for (i = 0; i < (tlvlen-8); i++){
        fft_mag = tlv[12 + i];
        printf("%d %d, ", i, fft_mag);
        fprintf(config->logfile,"%u ", fft_mag);
        if ((config->spectral_scan_dBm_adj==1) && (config->spectral_scan_pwr_format==1)){
            signed_fft_mag = fft_mag - 256;
            fprintf(config->logfile2,"%d %d \n", i, signed_fft_mag);
        } else {
            fprintf(config->logfile2,"%d %d \n", i, fft_mag);
        }
    }

    fclose(config->logfile2);
    config->logfile2 = NULL;

    printf("\n");
}

static int process_adc_report(logspectral_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen)
{
    printf("%s: Use application logadc for detailed logging. Hexdump follows:\n", __func__);
    hexdump(stdout, tlv, tlvlen);
    printf("\n");
}


#if defined(CONFIG_AR900B_SUPPORT)
/*
function : Start TLV capture in PHY debug memory
TODO    : Remove the magic numbers, shifts etc.
*/
static void start_ar900b_phy_debug()
{
    update_reg(dev, BB_PHYDBG_CONTROL1, PHYDBG_CAP_PRE_STORE,       0x0);
    update_reg(dev, BB_PHYDBG_CONTROL1, PHYDBG_PLYBCK_TRIG_MODE,    0x0);
    update_reg(dev, BB_PHYDBG_CONTROL1, PHYDBG_CAP_TRIG_MODE,       0x0);
    update_reg(dev, BB_PHYDBG_CONTROL1, PHYDBG_PLYBCK_COUNT,        0x0);
    update_reg(dev, BB_PHYDBG_CONTROL1, PHYDBG_PLYBCK_INC,          0x0);

    update_reg(dev, BB_PHYDBG_CONTROL2, PHYDBG_MODE,                0x7);
    update_reg(dev, BB_PHYDBG_CONTROL2, PHYDBG_PLYBCK_EN,           0x0);
    update_reg(dev, BB_PHYDBG_CONTROL2, PHYDBG_APB_AUTOINCR,        0x1);
    update_reg(dev, BB_PHYDBG_CONTROL2, PHYDBG_CAP_CHN_SEL,         0x0);
    update_reg(dev, BB_PHYDBG_CONTROL2, PHYDBG_CAP_TRIG_SELECT,     0x0);
    update_reg(dev, BB_PHYDBG_CONTROL2, PHYDBG_CAP_EN,              0x1);
}

/*
function: Stop TLV capture in PHY debug memory
TODO    : Remove the magic numbers, shifts etc.
*/
static void stop_ar900b_phy_debug()
{
    update_reg(dev, BB_PHYDBG_CONTROL2, PHYDBG_PLYBCK_EN, 0x0);
    update_reg(dev, BB_PHYDBG_CONTROL2, PHYDBG_CAP_EN,    0x0);
}

#define MAX_BUF_SIZE (30 * 1024)
#define DEFAULT_PHYDBG_FILENAME "ar900b_phymem.log"
#define NUM_OF_PHYDBG_MEM_ENTRIES   (23935)

/*
function: Dump AR900B PHY Debug memory
TODO    : Remove the magic numbers, shifts etc.
*/
static void dump_ar900b_phy_debug_mem()
{
    FILE* fd = NULL;
    u_int32_t *bufferWord;
    u_int32_t is_rx_tlv;
    u_int32_t tlv_start;
    u_int32_t tlv_data;
    u_int32_t i;
    u_int32_t value;
    u_int32_t num_rx_tlvs = 0;

    if ((fd = fopen(DEFAULT_PHYDBG_FILENAME, "wb+")) < 0) {
        printf("Unable to create/open file %s\n", DEFAULT_PHYDBG_FILENAME);
        exit(0);
    }

    fprintf(fd, "---------------------------------------------\n");
    fprintf(fd, "Short report at the end of the file\n");
    fprintf(fd, "---------------------------------------------\n");
    fprintf(fd, "\n");


    bufferWord = (u_int32_t*)MALLOC(MAX_BUF_SIZE);

    for (i = 0; i < NUM_OF_PHYDBG_MEM_ENTRIES; i++) {
        DiagReadWord(dev, PHY_BB_PHYDBG_MEM_DATA_ADDRESS, bufferWord);
        is_rx_tlv = ((*bufferWord) >> 31) & 0x1;
        tlv_start = ((*bufferWord) >> 30) & 0x1;
        tlv_data  = (*bufferWord) & 0x3fffffff;
        fprintf(fd, "%5d: is_rx_tlv=%d, tlv_start=%d, tlv_data=0x%x\n",i, is_rx_tlv, tlv_start, tlv_data);

        if (is_rx_tlv) {
            num_rx_tlvs++;
        }
    }

    fprintf(fd, "---------------------------------------------\n");
    fprintf(fd, "PHY_BB_PHYDBG_CONTROL1     = 0x%x\n", read_reg(dev, PHY_BB_PHYDBG_CONTROL1_ADDRESS, &value));
    fprintf(fd, "PHY_BB_PHYDBG_CONTROL2     = 0x%x\n", read_reg(dev, PHY_BB_PHYDBG_CONTROL2_ADDRESS, &value));
    fprintf(fd, "Number of RX TLVS          = %d\n", num_rx_tlvs);
    fprintf(fd, "---------------------------------------------\n");
    fclose(fd);
    free(bufferWord);

}

static void dw_configure_registers_ar900b(logspectral_config_t* config)
{
    /* Enable PHY Debug Memory capture */
    if (config->dump_phydbg_mem) {
       start_ar900b_phy_debug();
    }

     /* Editing note: Keep column alignment to help easy regex based changes */

    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ENA,            0x0);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ACTIVE,         0x0);


    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ENA,            0x1);

    update_mac_reg(dev, MAC_PCU_RX_FILTER,  PHY_DATA,                  0x1);
    update_mac_reg(dev, MAC_PCU_RX_FILTER,  PROMISCUOUS,               0x0);

    /* Enable All Error Mask */
    update_mac_reg(dev, MAC_PCU_PHY_ERROR_MASK, VALUE, 0xffffffff);
    update_mac_reg(dev, MAC_PCU_PHY_ERROR_MASK_CONT, VALUE, 0xffffffff);

    update_mac_reg(dev, MAC_PCU_PHY_DATA_LENGTH_THRESH, ENABLE, 0x1);
    update_mac_reg(dev, MAC_PCU_PHY_DATA_LENGTH_THRESH, VALUE, PHY_DATA_LENGTH_THRESH_VALUE);
    update_mac_reg(dev, OLE_RX_CONFIG_RING0, PHY_DATA_TYPE, 0x1);

    update_reg(dev, BB_EXTENSION_RADAR, RADAR_LB_DC_CAP, 60);

    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_FFT_SIZE,       config->spectral_scan_fft_size);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_COUNT,          config->spectral_scan_count);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_PERIOD,         config->spectral_scan_period);

    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_PRIORITY,       config->spectral_scan_priority);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_RESTART_ENA,    config->spectral_scan_restart_ena);

    update_reg(dev, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_DBM_ADJ,        config->spectral_scan_dBm_adj);
    update_reg(dev, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_RPT_MODE,       config->spectral_scan_rpt_mode);
    update_reg(dev, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_PWR_FORMAT,     config->spectral_scan_pwr_format);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_WB_RPT_MODE,    config->spectral_scan_wb_rpt_mode);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_RSSI_RPT_MODE,  config->spectral_scan_rssi_rpt_mode);
    update_reg(dev, BB_SRCH_FFT_CTRL_1, SPECTRAL_SCAN_BIN_SCALE,      config->spectral_scan_bin_scale);
    update_reg(dev, BB_SRCH_FFT_CTRL_4, SPECTRAL_SCAN_CHN_MASK,       config->spectral_scan_chn_mask);

    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_STR_BIN_THR,    config->spectral_scan_str_bin_thr);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_NB_TONE_THR,    config->spectral_scan_nb_tone_thr);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_INIT_DELAY,     config->spectral_scan_init_delay);
    update_reg(dev, BB_SPECTRAL_SCAN_2, SPECTRAL_SCAN_NOISE_FLOOR_REF,config->spectral_scan_noise_floor_ref); //default=0xa0 #0x96 for emulation due to the scaled down BW and the missing analog filter

    update_reg(dev, BB_SPECTRAL_SCAN_3, SPECTRAL_SCAN_RSSI_THR,       config->spectral_scan_rssi_thr);
    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_GC_ENA,         config->spectral_scan_gc_ena);

    /* Need to see if this is required */
    update_mac_reg(dev, MAC_PCU_PHY_ERR_CNT_1_MASK, VALUE, 0xffffffff);
    update_mac_reg(dev, MAC_PCU_PHY_ERR_CNT_1_MASK_CONT, VALUE, 0xffffffff);
    update_mac_reg(dev, MAC_PCU_PHY_ERR_CNT_2_MASK, VALUE, 0xffffffff);
    update_mac_reg(dev, MAC_PCU_PHY_ERR_CNT_2_MASK_CONT, VALUE, 0xffffffff);
    update_mac_reg(dev, MAC_PCU_PHY_ERR_CNT_3_MASK, VALUE, 0xffffffff);
    update_mac_reg(dev, MAC_PCU_PHY_ERR_CNT_3_MASK_CONT, VALUE, 0xffffffff);

    update_reg(dev, BB_SPECTRAL_SCAN,   SPECTRAL_SCAN_ACTIVE, 0x1);

    dump_spectral_register_params_ar900b();
}

static void dump_spectral_register_params_ar900b()
{
    u_int32_t val;
    u_int32_t reg_ss_1;
    u_int32_t reg_ss_2;
    u_int32_t reg_ss_3;
    u_int32_t fft_ctl_1;
    u_int32_t fft_ctl_2;
    u_int32_t fft_ctl_4;
	u_int32_t rx_error_mask;
	u_int32_t rx_error_mask_cont;
	u_int32_t rx_data;
	u_int32_t rx_filter;


    reg_ss_1 = read_reg(dev, PHY_BB_SPECTRAL_SCAN_ADDRESS, &val);

    printf("Enable                = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_ENA_GET(reg_ss_1));
    printf("Active                = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_ACTIVE_GET(reg_ss_1));
    printf("FFT Size              = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_FFT_SIZE_GET(reg_ss_1));
    printf("Scan Period           = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_PERIOD_GET(reg_ss_1));
    printf("Scan Count            = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_COUNT_GET(reg_ss_1));
    printf("Priority              = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_PRIORITY_GET(reg_ss_1));
    printf("GC Enable             = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_GC_ENA_GET(reg_ss_1));
    printf("Restart Enable        = %d\n", PHY_BB_SPECTRAL_SCAN_SPECTRAL_SCAN_RESTART_ENA_GET(reg_ss_1));

    reg_ss_2 = read_reg(dev, PHY_BB_SPECTRAL_SCAN_2_ADDRESS, &val);

    printf("Noise Floor ref (hex) = 0x%02x\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_NOISE_FLOOR_REF_GET(reg_ss_2));
    printf("Noise Floor ref       = %d\n", (signed char)PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_NOISE_FLOOR_REF_GET(reg_ss_2));
    printf("Init Delay            = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_INIT_DELAY_GET(reg_ss_2));
    printf("NB Tone thr           = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_NB_TONE_THR_GET(reg_ss_2));
    printf("String bin thr        = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_STR_BIN_THR_GET(reg_ss_2));
    printf("WB report mode        = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_WB_RPT_MODE_GET(reg_ss_2));
    printf("RSSI report           = %d\n", PHY_BB_SPECTRAL_SCAN_2_SPECTRAL_SCAN_RSSI_RPT_MODE_GET(reg_ss_2));

    reg_ss_3 = read_reg(dev, PHY_BB_SPECTRAL_SCAN_3_ADDRESS, &val);
    printf("RSSI Threshold (hex)  = 0x%02x\n", PHY_BB_SPECTRAL_SCAN_3_SPECTRAL_SCAN_RSSI_THR_GET(reg_ss_3));
    printf("RSSI Threshold        = %d\n", (signed char)PHY_BB_SPECTRAL_SCAN_3_SPECTRAL_SCAN_RSSI_THR_GET(reg_ss_3));

    fft_ctl_1 = read_reg(dev, PHY_BB_SRCH_FFT_CTRL_1_ADDRESS, &val);
    printf("PWR format            = %d\n", PHY_BB_SRCH_FFT_CTRL_1_SPECTRAL_SCAN_PWR_FORMAT_GET(fft_ctl_1));
    printf("RPT Mode              = %d\n", PHY_BB_SRCH_FFT_CTRL_1_SPECTRAL_SCAN_RPT_MODE_GET(fft_ctl_1));
    printf("Bin Scale             = %d\n", PHY_BB_SRCH_FFT_CTRL_1_SPECTRAL_SCAN_BIN_SCALE_GET(fft_ctl_1));
    printf("dBm Adjust            = %d\n", PHY_BB_SRCH_FFT_CTRL_1_SPECTRAL_SCAN_DBM_ADJ_GET(fft_ctl_1));

    fft_ctl_4 = read_reg(dev, PHY_BB_SRCH_FFT_CTRL_4_ADDRESS, &val);
    printf("Chain Mask            = %d\n", PHY_BB_SRCH_FFT_CTRL_4_SPECTRAL_SCAN_CHN_MASK_GET(fft_ctl_4));

    rx_error_mask = read_reg(dev, MAC_PCU_PHY_ERROR_MASK_ADDRESS, &val);
    printf("Error Mask            = 0x%x\n", MAC_PCU_PHY_ERROR_MASK_VALUE_GET(rx_error_mask));

    rx_error_mask_cont = read_reg(dev, MAC_PCU_PHY_ERROR_MASK_CONT_ADDRESS, &val);
    printf("Error Mask Cont       = 0x%x\n", MAC_PCU_PHY_ERROR_MASK_CONT_VALUE_GET(rx_error_mask_cont));

    rx_data = read_reg(dev, MAC_PCU_PHY_DATA_LENGTH_THRESH_ADDRESS, &val);
    printf("PHY Data Thresh       = 0x%x\n", MAC_PCU_PHY_DATA_LENGTH_THRESH_VALUE_GET(rx_data));
    printf("PHY Data Enable       = %d\n", MAC_PCU_PHY_DATA_LENGTH_THRESH_ENABLE_GET(rx_data));				

    rx_filter = read_reg(dev, MAC_PCU_RX_FILTER_ADDRESS, &val);
    printf("Rx Filter PHY data    = %d\n",  MAC_PCU_RX_FILTER_PHY_DATA_GET(rx_filter));
    printf("Rx Filter Promisuous  = %d\n",  MAC_PCU_RX_FILTER_PROMISCUOUS_GET(rx_filter));

#if DBG_DUMP_AR900B_REGISTER_VALUES
    print_spectral_reg_info_ar900b();
#endif
}

#define DEFAULT_IQ_FILENAME "ar900b_iq.samples"

/*
 * function : dump_adc_samples_ar900b
 *            Dump the IQ samples from ADC
 */
static void dump_adc_samples_ar900b()
{

#if 0
    u_int32_t i_sample;
    u_int32_t q_sample;
    u_int32_t i_sample_x;
    u_int32_t q_sample_x;
    u_int32_t i;
    u_int32_t j;
    FILE* fd = NULL;


    if ((fd = fopen(DEFAULT_IQ_FILENAME, "wb+")) < 0) {
        printf("Unable to create/open file %s\n", DEFAULT_IQ_FILENAME);
        exit(0);
    }

    update_reg(dev, BB_TEST_CONTROLS, CF_BBB_OBS_SEL, 0x1);
    update_reg(dev, BB_TEST_CONTROLS, RX_OBS_SEL_5TH_BIT, 0x0);
    update_reg(dev, BB_TEST_CONTROLS_STATUS, RX_OBS_SEL, 0x0);
    printf("ADC Samples I Q\n");
    for (i = 0; i <= 3; i++) {
        update_reg(dev, BB_ADDAC_CLK_SELECT, BB_ADC_CLK_SELECT_CH0, i);
        printf("DEBUG : ADC_CLK_SEL for next 10 ADC Samples: %x\n", i);
        for (j = 0; j < 10; j++) {
            PHY_BB_TSTADC_TSTADC_OUT_I_MSB;
            read_bb_reg_field(dev, BB_TSTADC, TSTADC_OUT_I_MSB, i_sample);
            read_bb_reg_field(dev, BB_TSTADC, TSTADC_OUT_Q_MSB, q_sample);
            i_sample_x = (i_sample ^ 0x200) - 512;
            q_sample_x = (q_sample ^ 0x200) - 512;
            fprintf(fd, "%x\t %x\t \n", i_sample, q_sample);
        }
    }
    fclose(fd);
#endif  /* 0 */
}

static void print_spectral_reg_info_ar900b()
{
    u_int32_t val;
    printf("PHY_BB_SPECTRAL_SCAN            = (0x%x)(0x%x)\n", PHY_BB_SPECTRAL_SCAN_ADDRESS,        read_reg(dev, PHY_BB_SPECTRAL_SCAN_ADDRESS, &val));
    printf("MAC_PCU_RX_FILTER               = (0x%x)(0x%x)\n", MAC_PCU_RX_FILTER_ADDRESS,           read_reg(dev, MAC_PCU_RX_FILTER_ADDRESS, &val));
    printf("MAC_PCU_PHY_ERROR_MASK          = (0x%x)(0x%x)\n", MAC_PCU_PHY_ERROR_MASK_ADDRESS,      read_reg(dev, MAC_PCU_PHY_ERROR_MASK_ADDRESS, &val));
    printf("MAC_PCU_PHY_ERROR_MASK_CONT     = (0x%x)(0x%x)\n", MAC_PCU_PHY_ERROR_MASK_CONT_ADDRESS, read_reg(dev, MAC_PCU_PHY_ERROR_MASK_CONT_ADDRESS, &val));
    printf("OLE_RX_CONFIG_RING0             = (0x%x)(0x%x)\n", OLE_RX_CONFIG_RING0_ADDRESS,         read_reg(dev, OLE_RX_CONFIG_RING0_ADDRESS, &val));
    printf("PHY_BB_EXTENSION_RADAR          = (0x%x)(0x%x)\n", PHY_BB_EXTENSION_RADAR_ADDRESS,      read_reg(dev, PHY_BB_EXTENSION_RADAR_ADDRESS, &val));
    printf("PHY_BB_SRCH_FFT_CTRL_1          = (0x%x)(0x%x)\n", PHY_BB_SRCH_FFT_CTRL_1_ADDRESS,      read_reg(dev, PHY_BB_SRCH_FFT_CTRL_1_ADDRESS, &val));
    printf("PHY_BB_SPECTRAL_SCAN_2          = (0x%x)(0x%x)\n", PHY_BB_SPECTRAL_SCAN_2_ADDRESS,      read_reg(dev, PHY_BB_SPECTRAL_SCAN_2_ADDRESS, &val));
    printf("PHY_BB_SPECTRAL_SCAN_3          = (0x%x)(0x%x)\n", PHY_BB_SPECTRAL_SCAN_3_ADDRESS,      read_reg(dev, PHY_BB_SPECTRAL_SCAN_3_ADDRESS, &val));
    printf("PHY_BB_SRCH_FFT_CTRL_4          = (0x%x)(0x%x)\n", PHY_BB_SRCH_FFT_CTRL_4_ADDRESS,      read_reg(dev, PHY_BB_SRCH_FFT_CTRL_4_ADDRESS, &val));
    printf("MAC_PCU_PHY_ERR_CNT_1_MASK      = (0x%x)(0x%x)\n", MAC_PCU_PHY_ERR_CNT_1_MASK_ADDRESS,  read_reg(dev, MAC_PCU_PHY_ERR_CNT_1_MASK_ADDRESS, &val));
    printf("MAC_PCU_PHY_ERR_CNT_1_MASK_CONT = (0x%x)(0x%x)\n", MAC_PCU_PHY_ERR_CNT_1_MASK_CONT_ADDRESS,  read_reg(dev, MAC_PCU_PHY_ERR_CNT_1_MASK_CONT_ADDRESS, &val));
    printf("MAC_PCU_PHY_ERR_CNT_2_MASK      = (0x%x)(0x%x)\n", MAC_PCU_PHY_ERR_CNT_2_MASK_ADDRESS,  read_reg(dev, MAC_PCU_PHY_ERR_CNT_2_MASK_ADDRESS, &val));
    printf("MAC_PCU_PHY_ERR_CNT_2_MASK_CONT = (0x%x)(0x%x)\n", MAC_PCU_PHY_ERR_CNT_2_MASK_CONT_ADDRESS,  read_reg(dev, MAC_PCU_PHY_ERR_CNT_2_MASK_CONT_ADDRESS, &val));
    printf("MAC_PCU_PHY_ERR_CNT_3_MASK      = (0x%x)(0x%x)\n", MAC_PCU_PHY_ERR_CNT_3_MASK_ADDRESS,  read_reg(dev, MAC_PCU_PHY_ERR_CNT_3_MASK_ADDRESS, &val));
    printf("MAC_PCU_PHY_ERR_CNT_3_MASK_CONT = (0x%x)(0x%x)\n", MAC_PCU_PHY_ERR_CNT_3_MASK_CONT_ADDRESS,  read_reg(dev, MAC_PCU_PHY_ERR_CNT_3_MASK_CONT_ADDRESS, &val));
}
#endif // CONFIG_AR900B_SUPPORT
