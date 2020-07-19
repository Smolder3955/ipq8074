/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/* Application to log ADC data from 11ac chipsets

   IMPORTANT:
   This is only temporary code that has been quickly written to
   validate ADC functionality. This code is NOT for
   any sort of production use and is likely to be in need
   of many improvements.

   Additionally, the code has been adapted from logspectral
   and may contain some vestiges that don't affect ADC
   functionality.

   TODO: Put common code in a library to be used across logspectral
   and logadc.
 */

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
#include "logadc.h"
#include "if_athioctl.h"
#include "ah.h"
#include "ah_internal.h"

#if defined(CONFIG_AR900B_SUPPORT)
/* Beeliner Headers */
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
/* Peregrine Headers */
#include "drivers/include/AR9888/hw/mac_pcu_reg.h"
#include "drivers/include/AR9888/hw/bb_reg_map.h"
#include "drivers/include/AR9888/hw/mac_descriptors/rx_ppdu_start.h"
#include "drivers/include/AR9888/hw/mac_descriptors/rx_ppdu_end.h"
#endif  /* CONFIG_AR900B_SUPPORT */
#include "drivers/include/spectral_adc_defs.h"

/* Enable Reg hack */
#define CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION 1
#define SPECTRAL_DATA_IN_TLV_FORMAT                 1

/* Global config - access this directly only where required. Else pass pointer */
logadc_config_t config;

/* device id */
int dev;


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

/* Options processing */
static const char *optString = "f:i:a:b:c:d:D:e:h?";

static const struct option longOpts[] = {
    { "file", required_argument, NULL, 'f' },
    { "device_name", required_argument, NULL, 'D' },
    { "interface", required_argument, NULL, 'i' },
    { "adc_capture_format", required_argument, NULL, 'a' },
    { "adc_capture_length", required_argument, NULL, 'b' },
    { "adc_capture_scale", required_argument, NULL, 'c' },
    { "adc_capture_gc_ena", required_argument, NULL, 'd' },
    { "adc_capture_chn_idx", required_argument, NULL, 'e' },
    { NULL, no_argument, NULL, 0 },
};


/* Static functions */
static int configure_registers(logadc_config_t *config);
static void signal_handler(int signal);
void cleanup(logadc_config_t *config);
static void hexdump(FILE* fp, unsigned char *buf, unsigned int len);
static int process_spectral_adc_sample(logadc_config_t *config);
static int obtain_adc_samples(logadc_config_t *config);
static void display_help();
static uint32_t regread(logadc_config_t *config, u_int32_t off);
static void regwrite(logadc_config_t *config, uint32_t off, uint32_t value);
static int process_spectral_summary_report(logadc_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen);
static int process_search_fft_report(logadc_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen);
static int process_adc_report(logadc_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen);
static int process_phy_tlv(logadc_config_t *config, uint32_t serial_num, void* buf);
static int process_rx_ppdu_start(logadc_config_t *config, uint32_t serial_num, struct rx_ppdu_start *ppdu_start);
static int process_rx_ppdu_end(logadc_config_t *config, uint32_t serial_num, struct rx_ppdu_end *ppdu_end);
static int dw_configure_registers(logadc_config_t *config);
void print_adc_config(logadc_config_t *config);

#define regupdate(config, REGISTERNAME, REGISTERFIELD, value) \
{\
    uint32_t regval;\
    regval= (regread((config), PHY_##REGISTERNAME##_ADDRESS) & (~PHY_##REGISTERNAME##_##REGISTERFIELD##_MASK)) | PHY_##REGISTERNAME##_##REGISTERFIELD##_SET((value));\
    regwrite((config), PHY_##REGISTERNAME##_ADDRESS, regval);\
}


/* Function definitions */

int main(int argc, char *argv[])
{
    int opt = 0;
    int longIndex;
    int err;
    char* dev_name = NULL;

    logadc_config_init(&config);

    opt = getopt_long(argc, argv, optString, longOpts, &longIndex);

    while( opt != -1 ) {
        switch(opt) {
            case 'f':
                strncpy(config.logfile_name, optarg, sizeof(config.logfile_name));
                break;

            case 'i':
                /* TODO: Validate input */
                strncpy(config.radio_ifname, optarg, sizeof(config.radio_ifname));
                break;

            case 'a':
                /* TODO: Validate input */
                config.adc_capture_format = atoi(optarg);
                break;

            case 'b':
                /* TODO: Validate input */
                config.adc_capture_length = atoi(optarg);
                break;

            case 'c':
                /* TODO: Validate input */
                config.adc_capture_scale = atoi(optarg);
                break;

            case 'd':
                /* TODO: Validate input */
                config.adc_capture_gc_ena = atoi(optarg);
                break;

            case 'D':
                dev_name = optarg;
                break;

            case 'e':
                /* TODO: Validate input */
                config.adc_capture_chn_idx = atoi(optarg);
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
    printf("ADC capture in another few seconds...");
    dev = dev_init(dev_name);
    sleep(1);
    dw_configure_registers(&config);
#else
    configure_registers(&config);
#endif

    //Carry out the main processing
    return obtain_adc_samples(&config);
}

#if CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION
static int dw_configure_registers(logadc_config_t *config)
{
    /* Editing note: Keep column alignment to help easy regex based changes */

    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_ACTIVE,     0x0);

    update_mac_reg(dev, MAC_PCU_RX_FILTER,  PHY_DATA,               0x0);
    update_mac_reg(dev, MAC_PCU_RX_FILTER,  PROMISCUOUS,            0x0);

    // Debug hack
    /* Program the PHY ERROR mask, remove magic number */
    write_reg(dev, 0x28338, 0x50);

    /* */
    write_reg(dev, 0x286b8, 0x10064);
    //write_reg(dev, 0x2803c, 1);

    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_LENGTH,     config->adc_capture_length);
    /* adc_capture_chn_idx: TBD on 3x3 */
    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_SCALE,      config->adc_capture_scale);
    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_FORMAT,     config->adc_capture_format);
    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_GC_ENA,     config->adc_capture_gc_ena);
    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_CHN_IDX,    config->adc_capture_chn_idx);

    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_ACTIVE,     0x1);

    print_adc_config(config);


}

static int dw_configure_ar900b_registers(logadc_config_t *config)
{
    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_ACTIVE,     0x0);

    update_mac_reg(dev, MAC_PCU_RX_FILTER,  PHY_DATA,               0x0);
    update_mac_reg(dev, MAC_PCU_RX_FILTER,  PROMISCUOUS,            0x0);

    update_mac_reg(dev, MAC_PCU_PHY_ERROR_MASK, VALUE, 0xffffffff);
    update_mac_reg(dev, MAC_PCU_PHY_ERROR_MASK_CONT, VALUE, 0xffffffff);
    update_mac_reg(dev, MAC_PCU_PHY_DATA_LENGTH_THRESH, ENABLE, 0x1);
    update_mac_reg(dev, MAC_PCU_PHY_DATA_LENGTH_THRESH, VALUE, PHY_DATA_LENGTH_THRESH_VALUE);

    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_LENGTH,     config->adc_capture_length);
    /* adc_capture_chn_idx: TBD on 3x3 */
    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_SCALE,      config->adc_capture_scale);
    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_FORMAT,     config->adc_capture_format);
    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_GC_ENA,     config->adc_capture_gc_ena);
    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_CHN_IDX,    config->adc_capture_chn_idx);

    update_reg(dev, BB_ADC_CAPTURE,     ADC_CAPTURE_ACTIVE,     0x1);

    print_adc_config(config);
}

#else   /* CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION */
static int configure_registers(logadc_config_t *config)
{
    /* Editing note: Keep column alignment to help easy regex based changes */

    regupdate(config, BB_ADC_CAPTURE,     ADC_CAPTURE_ACTIVE,     0x0);

    regupdate(config, MAC_PCU_RX_FILTER,  PHY_DATA,               0x1);
    regupdate(config, MAC_PCU_RX_FILTER,  PROMISCUOUS,            0x0);

    regupdate(config, BB_ADC_CAPTURE,     ADC_CAPTURE_LENGTH,     config->adc_capture_length);
    /* adc_capture_chn_idx: TBD on 3x3 */
    regupdate(config, BB_ADC_CAPTURE,     ADC_CAPTURE_SCALE,      config->adc_capture_scale);
    regupdate(config, BB_ADC_CAPTURE,     ADC_CAPTURE_FORMAT,     config->adc_capture_format);
    regupdate(config, BB_ADC_CAPTURE,     ADC_CAPTURE_GC_ENA,     config->adc_capture_gc_ena);

    regupdate(config, BB_ADC_CAPTURE,     ADC_CAPTURE_ACTIVE,     0x1);
}
#endif  /* CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION */

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

void read_adc_capture()
{
    int val;
    printf("\nadc capture       = 0x%x\n", read_reg(dev, PHY_BB_ADC_CAPTURE_ADDRESS, &val));
}

void read_macpcu_rx_filter()
{
    int val;
    printf("\nmacpcu rxfilter   = 0x%x\n", read_reg(dev, MAC_PCU_RX_FILTER_ADDRESS, &val));
}

void read_phy_data_len()
{
    int val;
    printf("\nphydata thresh   = 0x%x\n", read_reg(dev, MAC_PCU_PHY_DATA_LENGTH_THRESH_ADDRESS, &val));

}


void print_adc_config(logadc_config_t *config)
{
    printf("------------------Current adc configuration--------------------\n");
    printf("ADC_CAPTURE_FORMAT_DEFAULT = 0x%x ( %d )\n", config->adc_capture_format, config->adc_capture_format);
    printf("ADC_CAPTURE_LENGTH_DEFAULT = 0x%x ( %d )\n", config->adc_capture_length, config->adc_capture_length);
    printf("ADC_CAPTURE_SCALE_DEFAULT  = 0x%x ( %d )\n", config->adc_capture_scale, config->adc_capture_scale);
    printf("ADC_CAPTURE_GC_ENA         = 0x%x ( %d )\n", config->adc_capture_gc_ena, config->adc_capture_gc_ena);
    printf("ADC_CAPTURE_CHN_IDX        = 0x%x ( %d )\n", config->adc_capture_chn_idx, config->adc_capture_chn_idx);
    read_adc_capture();
    read_macpcu_rx_filter();
    read_phy_data_len();
    printf("------------------Current adc configuration--------------------\n");
}

void cleanup(logadc_config_t *config)
{
    read_adc_capture();
    read_macpcu_rx_filter();
#if CONFIGURE_SPECTRAL_VIA_DIRECT_REG_OPERATION
    update_reg(config, BB_ADC_CAPTURE,    ADC_CAPTURE_ACTIVE,     0x0);
#else
    regupdate(config, BB_ADC_CAPTURE,     ADC_CAPTURE_ACTIVE,     0x0);
#endif


    if (config->logfile != NULL) {
        fclose(config->logfile);
    }

    if (config->netlink_fd) {
        close(config->netlink_fd);
    }

    if (config->reg_fd) {
        close(config->reg_fd);
    }

    exit(0);
}

int logadc_config_init(logadc_config_t *config)
{
    if (NULL == config) {
        return -1;
    }

    config->logfile = NULL;
    strncpy(config->logfile_name, "/tmp/SS_adc_data.txt", sizeof (config->logfile_name));
    strncpy(config->radio_ifname, "wifi0", sizeof(config->radio_ifname));

    config->adc_capture_format =  ADC_CAPTURE_FORMAT_DEFAULT;
    config->adc_capture_length =  ADC_CAPTURE_LENGTH_DEFAULT;
    config->adc_capture_scale  =  ADC_CAPTURE_SCALE_DEFAULT;
    config->adc_capture_gc_ena =  ADC_CAPTURE_GC_ENA_DEFAULT;
    config->adc_capture_chn_idx = ADC_CAPTURE_CHN_IDX_DEFAULT;

    return 0;
}

static void hexdump(FILE *fp, unsigned char *buf, unsigned int len)
{
    while (len--) {
        fprintf(fp, "%02x", *buf++);
    }

    fprintf(fp, "\n");
}

static int process_spectral_adc_sample(logadc_config_t *config)
{
    struct nlmsghdr  *nlh = NULL;
    /* XXX Change this approach if we go multi-threaded!! */
    static char buf[NLMSG_SPACE(ADC_MAX_TLV_SIZE)];
    int bytes_read = 0;
    uint8_t *tempbuf;
    spectral_adc_log_desc *desc;

    nlh = (struct nlmsghdr *)buf;

    memset(nlh, 0, NLMSG_SPACE(sizeof(ADC_MAX_TLV_SIZE)));
    nlh->nlmsg_len  = NLMSG_SPACE(sizeof(ADC_MAX_TLV_SIZE));
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
#else


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
#endif  /* SPECTRAL_DATA_IN_TLV_FORMAT */

    return 0;
}

static int process_rx_ppdu_start(logadc_config_t *config, uint32_t serial_num, struct rx_ppdu_start *ppdu_start)
{
    printf("\n===Rx PPDU Start. Sr no.%u ===\n", serial_num);
#if defined(CONFIG_AR900B_SUPPORT)
    printf("\nrssi_pri_chain0 = %d, rssi_sec20_chain0 = %d, rssi_sec40_chain0 = %d, rssi_comb=%d\n", ppdu_start->rssi_pri_chain0, ppdu_start->rssi_sec20_chain0, ppdu_start->rssi_sec40_chain0, ppdu_start->rssi_comb);
#else   /* CONFIG_AR900B_SUPPORT */
    printf("\nrssi_chain0_pri20 = %d, rssi_chain0_sec20 = %d, rssi_chain0_sec40 = %d, rssi_comb=%d\n", ppdu_start->rssi_chain0_pri20, ppdu_start->rssi_chain0_sec20, ppdu_start->rssi_chain0_sec40, ppdu_start->rssi_comb);
#endif  /* CONFIG_AR900B_SUPPORT */
}

static int process_rx_ppdu_end(logadc_config_t *config, uint32_t serial_num, struct rx_ppdu_end *ppdu_end)
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


static int process_phy_tlv(logadc_config_t *config, uint32_t serial_num, void* buf)
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

static int obtain_adc_samples(logadc_config_t *config)
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

static void display_help()
{
    printf("\nlogadc v"LOGADC_VERSION": Log ADC data received from 11ac chipsets for debug purposes.\n"
           "\n"
           "Usage:\n"
           "\n"
           "logadc [OPTIONS]\n"
           "\n"
           "OPTIONS:\n"
           "\n"
           "-D, --device_name\n"
           "    Name of device\n"
           "\n"
           "-f, --file\n"
           "    Name of file to log ADC data to. Default: /tmp/SS_adc_data.txt\n"
           "\n"
           "-i, --interface\n"
           "    Name of radio interface. Default: wifi0\n"
           "\n"
           "--adc_capture_format\n"
           "    Value of adc_capture_format. Default: "WSTR(ADC_CAPTURE_FORMAT_DEFAULT)"\n"
           "\n"
           "--adc_capture_length. Default: "WSTR(ADC_CAPTURE_LENGTH_DEFAULT)"\n"
           "    Value of adc_capture_length.\n"
           "\n"
           "--adc_capture_scale. Default: "WSTR(ADC_CAPTURE_SCALE_DEFAULT)"\n"
           "    Value of adc_capture_scale.\n"
           "\n"
           "--adc_capture_gc_ena. Default: "WSTR(ADC_CAPTURE_GC_ENA_DEFAULT)"\n"
           "    Value of adc_capture_gc_ena.\n"
           "\n"
           "\n"
           "OTHER OPTIONS TO BE ADDED\n"
           "\n");
}

static uint32_t regread(logadc_config_t *config, u_int32_t off)
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

static void regwrite(logadc_config_t *config, uint32_t off, uint32_t value)
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

static int process_spectral_summary_report(logadc_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen)
{
    printf("%s: Use application logspectral for detailed logging. Hexdump follows:\n", __func__);
    hexdump(stdout, tlv, tlvlen);
    printf("\n");
}

static int process_search_fft_report(logadc_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen)
{
    printf("%s: Use application logspectral for detailed logging. Hexdump follows:\n", __func__);
    hexdump(stdout, tlv, tlvlen);
    printf("\n");
}


static int process_adc_report(logadc_config_t *config, uint32_t serial_num, uint8_t *tlv, uint32_t tlvlen)
{
    int i;
    uint32_t *pdata;
    uint32_t data;

    /* For simplicity, everything is defined as uint32_t (except one). Proper code will later use the right sizes. */
    uint32_t samp_fmt;
    uint32_t chn_idx;
    uint32_t recent_rfsat;
    uint32_t agc_mb_gain;
    uint32_t agc_total_gain;

    uint32_t *padc_summary;
    uint32_t adc_summary;

    printf("\n===ADC Report. Sr no.%u ===\n", serial_num);

    /* Relook this */
    if (tlvlen < 4) {
        fprintf(stderr, "Unexpected TLV length %d for ADC Report! Hexdump follows\n", tlvlen);
        hexdump(stderr, tlv, tlvlen + 4);
        return -1;
    }

    if ((config->logfile = fopen(config->logfile_name, "w")) == NULL) {
        perror("fopen");
        return -1;
    }

    padc_summary = (uint32_t*)(tlv + 4);
    adc_summary = *padc_summary;

    samp_fmt= ((adc_summary >> 28) & 0x1);
    chn_idx= ((adc_summary >> 24) & 0x3);
    recent_rfsat = ((adc_summary >> 23) & 0x1);
    agc_mb_gain = ((adc_summary >> 16) & 0x7f);
    agc_total_gain = adc_summary & 0x3ff;

    printf("samp_fmt= %u, chn_idx= %u, recent_rfsat= %u, agc_mb_gain=%u agc_total_gain=%u\n", samp_fmt, chn_idx, recent_rfsat, agc_mb_gain, agc_total_gain);

    for (i=0; i<(tlvlen/4); i++) {
        pdata = (uint32_t*)(tlv + 4 + i*4);
        data = *pdata;

        if (config->adc_capture_format == 1) {
            uint8_t i1;
            uint8_t q1;
            uint8_t i2;
            uint8_t q2;
            int8_t si1;
            int8_t sq1;
            int8_t si2;
            int8_t sq2;

            i1 = data & 0xff;
            q1 = (data >> 8 ) & 0xff;
            i2 = (data >> 16 ) & 0xff;
            q2 = (data >> 24 ) & 0xff;

            if (i1 > 127) {
                si1 = i1 - 256;
            } else {
                si1 = i1;
            }

            if (q1 > 127) {
                sq1 = q1 - 256;
            } else {
                sq1 = q1;
            }

            if (i2 > 127) {
                si2 = i2 - 256;
            } else {
                si2 = i2;
            }

            if (q2 > 127) {
                sq2 = q2 - 256;
            } else {
                sq2 = q2;
            }

            fprintf(config->logfile,"%d %d %d\n", 2*i, si1, sq1);
            fprintf(config->logfile, "%d %d %d\n", 2*i+1, si2, sq2);
        } else {
            uint16_t i1;
            uint16_t q1;
            int16_t si1;
            int16_t sq1;
            i1 = data & 0xffff;
            q1 = (data >> 16 ) & 0xffff;
            if (i1 > 32767) {
                si1 = i1 - 65536;
            } else {
                si1 = i1;
            }

            if (q1 > 32767) {
                sq1 = q1 - 65536;
            } else {
                sq1 = q1;
            }
            fprintf(config->logfile, "%d %d %d\n", i, si1, sq1);
        }
    }

    fclose(config->logfile);
    config->logfile = NULL;

    printf("\n");
}
