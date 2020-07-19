/*
 * Copyright (c) 2013-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010-2011, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * LMAC offload interface functions for UMAC - for power and performance offload model
 */
#include "ol_if_athvar.h"
#include "ol_if_athpriv.h"
#include <osdep.h>
#include "target_type.h"
#include <hif.h>
#include "bmi.h"
#include "sw_version.h"
#include "targaddrs.h"
#include "ol_helper.h"
#include "cdp_txrx_cmn.h"
#include "qdf_lock.h"  /* qdf_spinlock_* */
#include "qdf_types.h" /* qdf_vprint */
#include "qdf_util.h" /* qdf_vprint */
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "qdf_str.h"
#include "qdf_trace.h"
#include "wmi_unified_api.h"
#include "cdp_txrx_cmn_reg.h"
#include "wlan_lmac_if_def.h"
#include "wlan_lmac_if_api.h"
#include "wlan_mgmt_txrx_utils_api.h"
#include "cdp_txrx_ctrl.h"
#include "qwrap_structure.h"
#include <wlan_global_lmac_if_api.h>
#include "target_if.h"
#include "wlan_scan.h"
#include "ol_cfg.h"
#include "pld_common.h"

#ifdef QCA_PARTNER_PLATFORM
#include "ol_txrx_peer_find.h"
#endif
#include "target_if_scan.h"

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#endif
/* FIX THIS: the HL vs. LL selection will need to be done
 * at runtime rather than compile time
 */

#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"    /* TODO: check if we need a seperated config file */
#else
#include "wlan_tgt_def_config.h"
#endif
#include "dbglog_host.h"
#include "fw_dbglog_api.h"
#include <diag_event_log.h>
#include "ol_if_wow.h"
#include "a_debug.h"
#include "epping_test.h"

#include <init_deinit_lmac.h>

#include "pktlog_ac.h"
#include "ol_regdomain.h"
#include <wlan_reg_ucfg_api.h>
#include "ol_if_me.h"
#if WLAN_SPECTRAL_ENABLE
#include "ol_if_spectral.h"
#endif
#include "ol_ath.h"
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>

#include "ol_if_stats.h"
#include "ol_ratetable.h"
#include "wds_addr_api.h"

#include "qdf_atomic.h"
#include "ol_swap.h"

#include "ol_if_eeprom.h"
#include "ol_txrx_types.h"
#if ATH_PERF_PWR_OFFLOAD

#include "ath_pci.h"
#include <linux/fs.h>
#include <linux/gpio.h>
#ifndef __LINUX_POWERPC_ARCH__
#include <asm/segment.h>
#endif
#include <asm/uaccess.h>
#include <linux/buffer_head.h>
#include "reg_struct.h"
#include "regtable.h"

#if UMAC_SUPPORT_ACFG
#include "ieee80211_acfg.h"
#include <acfg_event_types.h>   /* for ACFG_WDT_TARGET_ASSERT */
#endif

#if WIFI_MEM_MANAGER_SUPPORT
#include "mem_manager.h"
#endif

#include <osif_private.h>

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_vdev_if.h>
#endif
#include "wlan_mgmt_txrx_utils_api.h"
#include <dispatcher_init_deinit.h>
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif
#if UNIFIED_SMARTANTENNA
#include <target_if_sa_api.h>
#endif /* UNIFIED_SMARTANTENNA */
#if WLAN_SPECTRAL_ENABLE
#include <wlan_spectral_ucfg_api.h>
#endif
#include <wlan_osif_priv.h>

#include <reg_services_public_struct.h>
#include <wlan_reg_services_api.h>
#include <ieee80211_regdmn.h>
#include <ol_regdomain_common.h>

#if UMAC_SUPPORT_CFG80211
#include <ieee80211_cfg80211.h>
#include "ol_ath_ucfg.h"
#endif

#include "pld_common.h"
#include <osif_fs.h>
#include "cfg_ucfg_api.h"

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif
#include <linux/of_fdt.h>

#include "AR900B/soc_addrs.h" /* tgt iram backup */
#ifdef CONFIG_AR900B_SUPPORT

/* TODO this section need to be used when linux kernel memory for crash
 * scope is increased
 */

#define FW_DUMP_FILE_QCA9888      "/lib/firmware/WLAN_FW_9888.BIN"
#define FW_DUMP_FILE_QCA9984      "/lib/firmware/WLAN_FW_9984.BIN"
#define FW_DUMP_FILE_AR900B       "/lib/firmware/WLAN_FW_900B.BIN"
#define FW_DUMP_FILE_AR9888       "/lib/firmware/WLAN_FW_9888.BIN"
#define FW_DUMP_FILE_IPQ4019      "/lib/firmware/WLAN_FW_IPQ4019.BIN"

/* WORDs, derived from AR600x_regdump.h */
#define REG_DUMP_COUNT_AR9888   60
#define REG_DUMP_COUNT_AR6320   60
#define REG_DUMP_COUNT_AR900B   60
#define REG_DUMP_COUNT_QCA9984  60
#define REG_DUMP_COUNT_QCA9888  60
#define REGISTER_DUMP_LEN_MAX   60

#if REG_DUMP_COUNT_AR9888 > REGISTER_DUMP_LEN_MAX
#error "REG_DUMP_COUNT_AR9888 too large"
#endif

#if REG_DUMP_COUNT_AR6320 > REGISTER_DUMP_LEN_MAX
#error "REG_DUMP_COUNT_AR6320 too large"
#endif

#if REG_DUMP_COUNT_AR900B > REGISTER_DUMP_LEN_MAX
#error "REG_DUMP_COUNT_AR900B too large"
#endif

#if REG_DUMP_COUNT_QCA9984 > REGISTER_DUMP_LEN_MAX
#error "REG_DUMP_COUNT_QCA9984 too large"
#endif

#if REG_DUMP_COUNT_QCA9888 > REGISTER_DUMP_LEN_MAX
#error "REG_DUMP_COUNT_QCA9888 too large"
#endif

#define FILE_PATH_LEN 128

#define TGT_IRAM_READ_PER_ITR (8 * 1024)    //Allowed Range [2,4,8,16,32]

extern unsigned int testmode;

uint8_t board_id = 0, chipid = 0;

struct dbglog_buf_s {
	uint32_t             next;
	uint32_t             buffer;
	uint32_t             bufsize;
	uint32_t             length;
	uint32_t             count;
	uint32_t             free;
} __attribute__ ((packed));

struct dbglog_hdr_s {
	struct dbglog_buf_s *dbuf;
	uint32_t             dropped;
} __attribute__ ((packed));

/* Following definitions are added from fw header bmi_msg.h.
 * Due to conflict in Host defined converged target_type.h
 * fw bmi_msg.h, this file includes host defined target_type.h.
 * If bmi_msg.h is included in this file then target_type value
 * conflicts and firmware loading fails.
 */
struct bmi_target_info {
    uint32_t target_info_byte_count; /* size of this structure */
    uint32_t target_ver;             /* Target Version ID */
    uint32_t target_type;            /* Target type */
};

#define BMI_SEGMENTED_WRITE_ADDR 0x1234

#ifdef ATH_AHB
#if OL_ATH_SUPPORT_LED
QDF_STATUS hif_diag_read_soc_ipq4019(struct hif_opaque_softc *hif_device,
			uint32_t address, uint8_t *data, int nbytes);
#endif
#endif

#define QC98XX_EEPROM_SIZE_LARGEST_AR900B   12064
#define QC98XX_EEPROM_SIZE_LARGEST_AR988X   2116
#define FLASH_CAL_START_OFFSET              0x1000

extern unsigned int ol_dual_band_5g_radios;
extern unsigned int ol_dual_band_2g_radios;

static uint32_t QC98XX_EEPROM_SIZE_LARGEST;

#ifdef AH_CAL_IN_FLASH_PCI
extern u_int32_t CalAddr[];
#endif


struct file* file_open(const char* path, int flags, int rights);
void file_close(struct file* file);

static int
get_fileindex(char *tmpbuf)
{
	char* filename = "/lib/firmware/.fileindex";
	return qdf_fs_read(filename, 0, 10, tmpbuf);
}

int filesnotexist = 0;
#define MAX_FILENAMES_SIZE 1024 * 4 //File list names len

static int
get_filenames(u_int32_t target_type,char *tmpbuf, int size)
{
    int ret;
    char* filename = NULL;

    switch (target_type)
    {
        case TARGET_TYPE_AR900B:
            if (chipid == 0) {
		filename = "/lib/firmware/AR900B/hw.1/.filenames";
            } else if (chipid == 1) {
		filename = "/lib/firmware/AR900B/hw.2/.filenames";
	    } else {
		qdf_info(KERN_ERR "%s: Chip id %d is not supported for AR900B!",__func__, chipid);
	        return -1;
	    }
	    break;
        case TARGET_TYPE_QCA9984:
	    if (chipid == 0) {
		filename = "/lib/firmware/QCA9984/hw.1/.filenames";
            } else {
                qdf_info(KERN_ERR "%s: Chip id %d is not supported for QCA9984!",__func__, chipid);
		return -1;
	    }
	    break;
	default:
	    qdf_info(KERN_ERR "%s: Target type %u is not supported.", __func__,target_type);
	    return -1;
    }
    /* Why ignore the return value of read_file()? It returns the number of
     * bytes that have been read. This is the number of valid bytes in the
     * buffer and exactly the number we need for the loop condition in
     * boardid_to_filename() */
    ret = qdf_fs_read(filename, 0, size, tmpbuf);
    return ret;
}

static int
boardid_to_filename(ol_ath_soc_softc_t *soc, int id, char *tmpbuf, int buflen, char *boarddata_file)
{
    int i, startindex = 0;
    char idstr[5] = {0};
    char *srcptr, *destptr;
    long result = 0;
    int len = 0, found = -1;
    uint32_t target_type;

    startindex = 0;
    target_type = lmac_get_tgt_type(soc->psoc_obj);

    for(i = 0; i < (buflen - 4); i++){
        if(tmpbuf[i] == '.') {
            idstr[0]= tmpbuf[i-3];
            idstr[1]= tmpbuf[i-2];
            idstr[2]= tmpbuf[i-1];
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39)
            strict_strtol(idstr, 0, &result);
#else
            /* As we are reading arbitrary file content, it is not unlikely that
             * kstrtol fails. Therefore check the return value. */
            if (kstrtol(idstr, 10, &result) != 0) {
                qdf_info(KERN_ERR "%s: Failed to convert '%s' to integer!", __func__, idstr);
                return -1;
            }
#endif
            if(result == id && result != 0){
                srcptr = &tmpbuf[startindex];
                destptr = &boarddata_file[0];
                len    = i + 4 - startindex;
                qdf_str_lcopy(destptr, srcptr, len + 1);
                boarddata_file[i + 4 - startindex] = '\0';
                found = 1;
                break;
            }
            else{
                i+=4;
                startindex = i + 1;
                continue;
            }
        }/* buffer */
    } /* for loop */

    /*
     * instead of silently falling back to a default image. Otherwise we may run
     *        * into issues during operation that unnecessarily consume debugging
     *               * resources because they are very hard to relate to wrong board data. */
    if (found != 1) {
        if (target_type == TARGET_TYPE_IPQ4019) {
            destptr = &boarddata_file[0];
            switch(id)
            {
                case DK01_2G_Y9803:
                    srcptr = IPQ4019_DK01_Y9803_2G_BOARD_DATA_FILE;
                    break;
                case DK01_5G_Y9803:
                    srcptr = IPQ4019_DK01_Y9803_5G_BOARD_DATA_FILE;
                    break;
                case DK03_2G:
                    srcptr = IPQ4019_DK03_2G_BOARD_DATA_FILE;
                    break;
                case DK03_5G:
                    srcptr = IPQ4019_DK03_5G_BOARD_DATA_FILE;
                    break;
                case DK04_2G:
                    srcptr = IPQ4019_DK04_2G_BOARD_DATA_FILE;
                    break;
                case DK04_5G:
                    srcptr = IPQ4019_DK04_5G_BOARD_DATA_FILE;
                    break;
                case DK04_NEGATIVE_POWER_2G:
                    srcptr = IPQ4019_DK04_NEGATIVE_POWER_2G_BOARD_DATA_FILE;
                    break;
                case DK04_NEGATIVE_POWER_5G:
                    srcptr = IPQ4019_DK04_NEGATIVE_POWER_5G_BOARD_DATA_FILE;
                    break;
                case DK03_YA131_2G:
                    srcptr = IPQ4019_DK03_YA131_2G_BOARD_DATA_FILE;
                    break;
                case DK03_YA131_5G:
                    srcptr = IPQ4019_DK03_YA131_5G_BOARD_DATA_FILE;
                    break;
                case DK05_2G:
                    srcptr = IPQ4019_DK05_2G_BOARD_DATA_FILE;
                    break;
                case DK05_5G:
                    srcptr = IPQ4019_DK05_5G_BOARD_DATA_FILE;
                    break;
                case DK07_2G:
                    srcptr = IPQ4019_DK07_2G_BOARD_DATA_FILE;
                    break;
                case DK07_5G_HB:
                    srcptr = IPQ4019_DK07_5G_HB_BOARD_DATA_FILE;
                    break;
                case DK07_5G_LB:
                    srcptr = IPQ4019_DK07_5G_LB_BOARD_DATA_FILE;
                    break;
                case DK06_2G:
                    srcptr = IPQ4019_DK06_2G_BOARD_DATA_FILE;
                    break;
                case DK06_5G:
                    srcptr = IPQ4019_DK06_5G_BOARD_DATA_FILE;
                    break;
                case DK08_2G:
                    srcptr = IPQ4019_DK08_2G_BOARD_DATA_FILE;
                    break;
                case DK08_5G_LB:
                    srcptr = IPQ4019_DK08_5G_LB_BOARD_DATA_FILE;
                    break;

                default:
                    qdf_info("Warning : No board id matched. Selecting default boarddata file");
                    if(soc->soc_idx == 0)
                        srcptr = "boarddata_0.bin";
                    else
                        srcptr = "boarddata_1.bin";
                    break;
            }
            qdf_str_lcopy(destptr, srcptr, strlen(srcptr) + 1);
            qdf_info("\n %s: Selecting board data file name %s",soc->sc_osdev->netdev->name, destptr);
        }
        else if (target_type == TARGET_TYPE_QCA9984) {
            destptr = &boarddata_file[0];
            if (soc->soc_idx == 0) {
                qdf_str_lcopy(destptr, "boarddata_0.bin", 16);
                qdf_info("\n wifi0: Selecting board data file name %s", destptr);
            } else if (soc->soc_idx == 1) {
                qdf_str_lcopy(destptr, "boarddata_1.bin", 16);
                qdf_info("\n wifi1: Selecting board data file name %s", destptr);
            } else if (soc->soc_idx == 2) {
                qdf_str_lcopy(destptr, "boarddata_2.bin", 16);
                qdf_info("\n wifi2: Selecting board data file name %s", destptr);
            } else {
                qdf_info("\n Unable to map board data file for unknown device "
                       "%s",
                       soc->sc_osdev->netdev->name);
                return -1;
            }
        } else if (target_type == TARGET_TYPE_QCA9888){
            destptr = &boarddata_file[0];
            switch(id)
            {
                case QCA9888_Y9690_VER2:
                    srcptr = QCA9888_Y9690_VER2_BOARD_DATA_FILE;
                    break;
                case QCA9888_Y9484_VER2:
                    srcptr = QCA9888_Y9484_VER2_BOARD_DATA_FILE;
                    break;
                case QCA9888_Y9582_VER2:
                    srcptr = QCA9888_Y9582_VER2_BOARD_DATA_FILE;
                    break;
                case QCA9888_Y9690_VER2_SBS_HB:
                    srcptr = QCA9888_Y9690_VER2_SBS_HB_BOARD_DATA_FILE;
                    break;
                case QCA9888_YA105_VER2:
                    srcptr = QCA9888_YA105_VER2_BOARD_DATA_FILE;
                    break;
                case QCA9888_YA841_VER2:
                    srcptr = QCA9888_YA841_VER2_BOARD_DATA_FILE;
                    break;
                default:
                    qdf_info("Warning : No board id matched. Selecting default boarddata file");
                    srcptr = "boarddata_0.bin";
                    break;
            }

            qdf_str_lcopy(destptr, srcptr, strlen(srcptr) + 1);
            qdf_info("\n %s: Selecting board data file name %s",soc->sc_osdev->netdev->name, destptr);
	}else {
            qdf_info("\n ******************************************* CAUTION!! ************************************************");
            qdf_info(KERN_ERR "%s: Invalid BoardId! Did not find board data file for board id %d", __func__, id);
            destptr = &boarddata_file[0];
            if (soc->soc_idx == 0) {
                qdf_str_lcopy(destptr, DEFAULT_BOARDDATA_FILE_5G, strlen(DEFAULT_BOARDDATA_FILE_5G) + 1);
            } else if (soc->soc_idx == 1) {
                qdf_str_lcopy(destptr, DEFAULT_BOARDDATA_FILE_2G, strlen(DEFAULT_BOARDDATA_FILE_2G) + 1);
            } else {
                qdf_str_lcopy(destptr, DEFAULT_BOARDDATA_FILE_5G, strlen(DEFAULT_BOARDDATA_FILE_5G) + 1);
            }
            qdf_info("\n ******************************LOADING DEFAULT BOARD DATA FILE************************");

        }
    }


return 0;

}

static int
update_fileindex(int fileindex)
{
	char tmpbuf[20] = {-1};
	char* filename = NULL;
	filename = "/lib/firmware/.fileindex";

	snprintf(tmpbuf, sizeof(tmpbuf), "%c\n",fileindex);
	if (filename) {
		qdf_fs_write(filename, 0, 4, tmpbuf);
	}

	return 0;
}
#endif /* CONFIG_AR900B_SUPPORT */

int
ol_ath_configure_target(ol_ath_soc_softc_t *soc)
{
    u_int32_t param;
    uint32_t target_type;
    void *hif_hdl;
    struct bmi_info *bmi_handle;

    hif_hdl = lmac_get_hif_hdl(soc->psoc_obj);
    if (!hif_hdl)
    {
	    qdf_info("Failed to get valid hif_hdl ");
	    return A_ERROR;
    }
    target_type =  lmac_get_tgt_type(soc->psoc_obj);
    bmi_handle = soc->bmi_handle;

#define HTC_PROTOCOL_VERSION   0x0002
    /* Tell target which HTC version it is used*/
    param = HTC_PROTOCOL_VERSION;
    if (BMIWriteMemory(hif_hdl,
                       host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_app_host_interest)),
                       (u_int8_t *)&param, 4, bmi_handle)!= A_OK)
    {
         qdf_info("BMIWriteMemory for htc version failed ");
         return -1;
    }

    /* set the firmware mode to STA/IBSS/AP */
    {
        if (BMIReadMemory(hif_hdl,
            host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_option_flag)),
            (A_UCHAR *)&param,
            4, bmi_handle)!= A_OK)
        {
            qdf_info("BMIReadMemory for setting fwmode failed ");
            return A_ERROR;
        }

    /* TODO following parameters need to be re-visited. */
        param |= (1 << HI_OPTION_NUM_DEV_SHIFT); //num_device
        param |= (HI_OPTION_FW_MODE_AP << HI_OPTION_FW_MODE_SHIFT); //Firmware mode ??
        param |= (1 << HI_OPTION_MAC_ADDR_METHOD_SHIFT); //mac_addr_method
        param |= (0 << HI_OPTION_FW_BRIDGE_SHIFT);  //firmware_bridge
        param |= (0 << HI_OPTION_FW_SUBMODE_SHIFT); //fwsubmode

        qdf_info(KERN_INFO"NUM_DEV=%d FWMODE=0x%x FWSUBMODE=0x%x FWBR_BUF %d",
                            1, HI_OPTION_FW_MODE_AP, 0, 0);

        if (BMIWriteMemory(hif_hdl,
            host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_option_flag)),
            (A_UCHAR *)&param,
            4, bmi_handle) != A_OK)
        {
            qdf_info("BMIWriteMemory for setting fwmode failed ");
            return A_ERROR;
        }
    }

#if (CONFIG_DISABLE_CDC_MAX_PERF_WAR)
    {
        /* set the firmware to disable CDC max perf WAR */
        if (BMIReadMemory(hif_hdl,
            host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_option_flag2)),
            (A_UCHAR *)&param,
            4, bmi_handle)!= A_OK)
        {
            qdf_info("BMIReadMemory for setting cdc max perf failed ");
            return A_ERROR;
        }

        param |= HI_OPTION_DISABLE_CDC_MAX_PERF_WAR;
        if (BMIWriteMemory(hif_hdl,
            host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_option_flag2)),
            (A_UCHAR *)&param,
            4, bmi_handle) != A_OK)
        {
            qdf_info("BMIWriteMemory for setting cdc max perf failed ");
            return A_ERROR;
        }
    }
#endif /* CONFIG_CDC_MAX_PERF_WAR */

    /* If host is running on a BE CPU, set the host interest area */
    {
#if defined(BIG_ENDIAN_HOST) && !AH_NEED_TX_DATA_SWAP
        param = 1;
#else
            param = 0;
#endif
        if (BMIWriteMemory(hif_hdl,
            host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_be)),
            (A_UCHAR *)&param,
            4, bmi_handle) != A_OK)
        {
            qdf_info("BMIWriteMemory for setting host CPU BE mode failed ");
            return A_ERROR;
        }
    }

    /* FW descriptor/Data swap flags */
    {
        param = 0;
        if (BMIWriteMemory(hif_hdl,
            host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_fw_swap)),
            (A_UCHAR *)&param,
            4, bmi_handle) != A_OK)
        {
            qdf_info("BMIWriteMemory for setting FW data/desc swap flags failed ");
            return A_ERROR;
        }
    }
    /*Set the TX/RX DATA SWAP mode, if not define any AH_NEED_TX_DATA_SWAP and AH_NEED_RX_DATA_SWAP
	  The vlaue should be zero, that Target will not  enable TX_PACKET_BYTE_SWAP in  MAC_DMA_CFG
	  and PACKET_BYTE_SWAP and HEADER_BYTE_SWAP for MAC_DMA_RCV_RING2_2 */
    {
        param = 0;
#if AH_NEED_TX_DATA_SWAP
        param |= 0x01;
#endif
#if AH_NEED_RX_DATA_SWAP
        param |= 0x02;
#endif
#if ATH_11AC_ACK_POLICY
        param |= 0x04;
#endif
        if (BMIWriteMemory(hif_hdl,
            host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_txrx_dataswap)),
            (A_UCHAR *)&param,
            4, bmi_handle) != A_OK)
        {
            qdf_info("BMIWriteMemory for setting host TX/RX SWAP mode failed ");
            return A_ERROR;
        }
    }

    return A_OK;
}

int
ol_check_dataset_patch(ol_ath_soc_softc_t *soc, u_int32_t *address)
{
    /* Check if patch file needed for this target type/version. */
    return 0;
}

static int
ol_validate_otp_mod_param(uint32_t param)
{

    /* Note this sanity checks only module param given or not
     * Its user responsibility to properly feed
     * module param. This is intentionally done.
     */
    if(param == 0xffffffff)
        return -1;

    return 0;
}

#define I2C_SDA_GPIO_PIN    5
#define I2C_SDA_PIN_CONFIG  3
#define SI_CLK_GPIO_PIN     17
#define SI_CLK_PIN_CONFIG   3
void config_target_eeprom(ol_ath_soc_softc_t *soc)
{
    struct hif_opaque_softc *hif_hdl;
    struct hif_softc *scn;

    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);

    if (!hif_hdl)
    {
	    qdf_info("Failed to get valid hif_hdl ");
	    return;
    }
    scn = HIF_GET_SOFTC(hif_hdl);
    /* Enable SI clock */
    hif_reg_write(hif_hdl, (RTC_SOC_BASE_ADDRESS + CLOCK_CONTROL_OFFSET), 0x0);

    /* Configure GPIOs for I2C operation */
    hif_reg_write(hif_hdl,(GPIO_BASE_ADDRESS + GPIO_PIN0_OFFSET + I2C_SDA_GPIO_PIN*4),
                     (WLAN_GPIO_PIN0_CONFIG_SET(I2C_SDA_PIN_CONFIG) |
                      WLAN_GPIO_PIN0_PAD_PULL_SET(1)));

    hif_reg_write(hif_hdl, (GPIO_BASE_ADDRESS + GPIO_PIN0_OFFSET + SI_CLK_GPIO_PIN*4),
                     (WLAN_GPIO_PIN0_CONFIG_SET(SI_CLK_PIN_CONFIG) |
                      WLAN_GPIO_PIN0_PAD_PULL_SET(1)));

    hif_reg_write(hif_hdl,(GPIO_BASE_ADDRESS + GPIO_ENABLE_W1TS_LOW_ADDRESS),
                      1 << SI_CLK_GPIO_PIN);

    /* In Swift ASIC - EEPROM clock will be (110MHz/512) = 214KHz */
    hif_reg_write(hif_hdl, (SI_BASE_ADDRESS + SI_CONFIG_OFFSET),
                      (SI_CONFIG_ERR_INT_SET(1) |
                      SI_CONFIG_BIDIR_OD_DATA_SET(1) |
                      SI_CONFIG_I2C_SET(1) |
                      SI_CONFIG_POS_SAMPLE_SET(1) |
                      SI_CONFIG_INACTIVE_CLK_SET(1) |
                      SI_CONFIG_INACTIVE_DATA_SET(1) |
                      SI_CONFIG_DIVIDER_SET(8)));

}

#define SI_OK                               1
#define SI_ERR                              0
#define DEVICE_SELECT                       0xa0
#define DEVICE_READ                         0xa1000000
#define EEPROM_ADDR_OFFSET_LEN              16 /* in bits */
#define EEPROM_ADDR_OFFSET_LOWER_BYTE_MASK  0x00ff
#define EEPROM_ADDR_OFFSET_UPPER_BYTE_MASK  0xff00

#define MAX_WAIT_COUNTER_POLL_DONE_BIT      100000 /* 1 sec(100000 * 10 usecs) */
#define DELAY_BETWEEN_DONE_BIT_POLL         10     /* In usecs */
static int eeprom_byte_read(ol_ath_soc_softc_t *soc, u_int16_t addr_offset, u_int8_t *data)
{
    struct hif_opaque_softc *hif_hdl;
    struct hif_softc *scn;
    u_int32_t reg;
    int wait_limit;

    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);
    if (!hif_hdl)
    {
	    qdf_info("Failed to get valid hif_hdl ");
	    return SI_ERR;
    }
    scn = HIF_GET_SOFTC(hif_hdl);
    /* set device select byte and for the read operation */
    reg = DEVICE_SELECT |
          ((addr_offset & EEPROM_ADDR_OFFSET_LOWER_BYTE_MASK) << EEPROM_ADDR_OFFSET_LEN) |
          (addr_offset & EEPROM_ADDR_OFFSET_UPPER_BYTE_MASK) |
          DEVICE_READ ;
    hif_reg_write(hif_hdl, SI_BASE_ADDRESS + SI_TX_DATA0_OFFSET, reg);

    /* write transmit data, transfer length, and START bit */
    hif_reg_write(hif_hdl, SI_BASE_ADDRESS + SI_CS_OFFSET,
                                (SI_CS_START_SET(1) |
                                 SI_CS_RX_CNT_SET(1) |
                                 SI_CS_TX_CNT_SET(4)));

    wait_limit = MAX_WAIT_COUNTER_POLL_DONE_BIT;
    /* poll CS_DONE_INT bit */
    reg = hif_reg_read(hif_hdl, SI_BASE_ADDRESS + SI_CS_OFFSET);
    /* Wait for maximum 1 sec */
    while((wait_limit--) && ((reg & SI_CS_DONE_INT_MASK) != SI_CS_DONE_INT_MASK)) {
            OS_DELAY(DELAY_BETWEEN_DONE_BIT_POLL);
            reg = hif_reg_read(hif_hdl, SI_BASE_ADDRESS + SI_CS_OFFSET);
    }
    if(wait_limit == 0) {
        qdf_info("%s: Timeout waiting for DONE_INT bit to be set in SI_CONFIG register", __func__);
        return SI_ERR;
    }

    /*
     * Clear DONE_INT bit
     * DONE_INT bit is cleared when 1 is written to this field (or) when the
     * START bit is set in this register
     */
    hif_reg_write(hif_hdl, SI_BASE_ADDRESS + SI_CS_OFFSET, reg);

    if((reg & SI_CS_DONE_ERR_MASK) == SI_CS_DONE_ERR_MASK)
    {
        return SI_ERR;
    }

    /* extract receive data */
    reg = hif_reg_read(hif_hdl, SI_BASE_ADDRESS + SI_RX_DATA0_OFFSET);
    *data = (reg & 0xff);

    return SI_OK;
}

static int qc98xx_verify_checksum(void *eeprom)
{
    uint16_t *p_half;
    uint16_t sum = 0;
    int i;

    p_half = (uint16_t *)eeprom;
    for (i = 0; i < QC98XX_EEPROM_SIZE_LARGEST / 2; i++) {
        sum ^= le16_to_cpu(*p_half++);
    }
    if (sum != 0xffff) {
        qdf_info("%s error: flash checksum 0x%x, computed 0x%x ", __func__,
                le16_to_cpu(*((uint16_t *)eeprom + 1)), sum ^ 0xFFFF);
        return -1;
    }
    qdf_info("%s: flash checksum passed: 0x%4x", __func__, le16_to_cpu(*((uint16_t *)eeprom + 1)));
    return 0;
}

int
ol_transfer_target_eeprom_caldata(ol_ath_soc_softc_t *soc, u_int32_t address, bool compressed)
{
    int status = EOK;
    struct firmware_priv fwtemp = {0};
    struct firmware_priv *fw_entry = &fwtemp;
    u_int32_t fw_entry_size, orig_size = 0;
    int i;
    uint32_t *savedestp, *destp, *srcp = NULL;
    u_int8_t *pdst, *psrc,*ptr = NULL;

    struct hif_opaque_softc *sc;
    uint32_t target_type;
    uint32_t target_version;

    sc = lmac_get_ol_hif_hdl(soc->psoc_obj);
    if(!sc) {
        qdf_info("ERROR: sc ptr is NULL");
        return -EINVAL;
    }
    target_type = lmac_get_tgt_type(soc->psoc_obj);
    target_version = lmac_get_tgt_version(soc->psoc_obj);
    if (target_type == TARGET_TYPE_AR9888) {
        QC98XX_EEPROM_SIZE_LARGEST = QC98XX_EEPROM_SIZE_LARGEST_AR988X;
    } else {
        QC98XX_EEPROM_SIZE_LARGEST = QC98XX_EEPROM_SIZE_LARGEST_AR900B;
    }

    // Check for target/host driver mismatch
    if (target_version != AR9887_REV1_VERSION) {
        qdf_info("ERROR: UNSUPPORTED TARGET VERSION 0x%x ", target_version);
        return EOK;
    }
    else {
        u_int8_t *tmp_ptr;
        u_int16_t addr_offset;
        ptr = vmalloc(QC98XX_EEPROM_SIZE_LARGEST);
        if ( NULL == ptr ){
        qdf_info("%s %d: target eeprom caldata memory allocation failed",__func__, __LINE__);
        return -EINVAL;
        } else {
            tmp_ptr = ptr;
            /* Config for Target EEPROM access */
            config_target_eeprom(soc);
            /* For Swift 2116 bytes of caldata is stored in target */
            for(addr_offset = 0;  addr_offset < QC98XX_EEPROM_SIZE_LARGEST; addr_offset++) {
                if(eeprom_byte_read(soc, addr_offset, tmp_ptr) != SI_OK) {
                    if (ptr)
                        vfree(ptr);
                    return -EINVAL;
                }
                //qdf_info("%s: addr_offset %d value %02x", __func__,addr_offset,*tmp_ptr);
                tmp_ptr++;
            }
            if (le16_to_cpu(*(uint16_t *)ptr) != QC98XX_EEPROM_SIZE_LARGEST) {
                qdf_info("%s: Target EEPROM caldata len %d doesn't equal to %d", __func__,
                        le16_to_cpu(*(uint16_t *)ptr), QC98XX_EEPROM_SIZE_LARGEST);
                if (ptr)
                    vfree(ptr);
                return -EINVAL;
            }
            if (qc98xx_verify_checksum(ptr)){
                if (ptr)
                    vfree(ptr);
                return -EINVAL;
            }
            srcp = (uint32_t *)ptr;
            orig_size = QC98XX_EEPROM_SIZE_LARGEST;
            fw_entry->data = ptr;
            fw_entry->size = (orig_size + 3) & ~3;
            qdf_info("%s %d: Download Target EEPROM caldata len %zd",
                    __func__, __LINE__, fw_entry->size);

            savedestp = destp = vmalloc(fw_entry->size);
            if(destp == NULL)
            {
                if (ptr)
                    vfree(ptr);
                qdf_info("%s %d: memory allocation failed",__func__, __LINE__);
                return -EINVAL;
            }
            pdst = (uint8_t *)destp;
            psrc = (uint8_t *)srcp;

            /* Add pad bytes if required */
            for (i = 0; i < fw_entry->size; i++) {
                if (i < orig_size)
                    pdst[i] = psrc[i];
                else
                    pdst[i] = 0;
            }
            for(i=0; i < (fw_entry->size)/4; i++) {
                *destp = cpu_to_le32(*srcp);
                destp++; srcp++;
            }

            destp = savedestp;
            fw_entry_size = fw_entry->size;

            if (compressed) {
                status = BMIFastDownload(sc, address, (u_int8_t *)destp, fw_entry_size, soc->bmi_handle);
            } else {
                status = BMIWriteMemory(sc, address, (u_int8_t *)destp, fw_entry_size, soc->bmi_handle);
            }
        }

        if (status != EOK) {
            qdf_info("%s :%d BMI operation failed ",__func__, __LINE__);
        }

        if (ptr)
            vfree(ptr);

        if(destp)
            vfree(destp);

        if (status != EOK) {
            return -1;
        }
      return status;
    }
}


#define MAX_FILENAME_BUFF_SIZE  256
#define MAX_ABSOLUTE_FILE_SIZE  512

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,4,103)
#ifndef CALDATA0_FILE_PATH
#define CALDATA0_FILE_PATH	"/tmp/wifi0.caldata"
#endif

#ifndef CALDATA1_FILE_PATH
#define CALDATA1_FILE_PATH	"/tmp/wifi1.caldata"
#endif

#ifndef CALDATA2_FILE_PATH
#define CALDATA2_FILE_PATH	"/tmp/wifi2.caldata"
#endif
#endif
enum {
    ESKIP_OTP = 50, // A value greater than STD error codes
    ESKIP_FW,
    ESKIP_BOARD
};
int
ol_transfer_bin_file(ol_ath_soc_softc_t *soc, ATH_BIN_FILE file,
                    u_int32_t address, bool compressed)
{
    int status = EOK;
    const char *filename = NULL;
    const char *pathname = NULL;
    struct firmware_priv *fw_entry = NULL;
    u_int32_t offset;
    u_int32_t fw_entry_size;
    u_int8_t *tempEeprom = NULL;
    u_int32_t board_data_size = 0;
    u_int8_t *ptr = NULL;
    uint32_t *srcp = NULL;
    uint32_t orig_size = 0;
    char buf[MAX_FILENAME_BUFF_SIZE];
    char absolute_filename[MAX_ABSOLUTE_FILE_SIZE] ;
    int i = 0;
    uint32_t *savedestp = NULL, *destp = NULL;
    u_int8_t *pad_dst = NULL, *pad_src = NULL;
    int rfwst=0;
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;
    uint32_t target_type;
    uint32_t target_version;
    uint32_t target_revision;
    uint32_t emu_type;
    struct target_psoc_info *tgt_psoc_info;
    void *hif_hdl;
    struct bmi_info *bmi_handle;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                                        soc->psoc_obj);
    if(tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return -1;
    }

    emu_type = cfg_get(psoc, CFG_OL_EMU_TYPE);
    target_type = target_psoc_get_target_type(tgt_psoc_info);
    target_version = target_psoc_get_target_ver(tgt_psoc_info);
    target_revision = target_psoc_get_target_rev(tgt_psoc_info);
    hif_hdl = target_psoc_get_hif_hdl(tgt_psoc_info);
    /*
     * set caldata length based upon current target type.
     */
    if (target_type == TARGET_TYPE_AR9888) {
        QC98XX_EEPROM_SIZE_LARGEST = QC98XX_EEPROM_SIZE_LARGEST_AR988X;
    } else {
        QC98XX_EEPROM_SIZE_LARGEST = QC98XX_EEPROM_SIZE_LARGEST_AR900B;
    }
    bmi_handle = soc->bmi_handle;

    switch (file)
    {
        default:
            qdf_info("%s: Unknown file type", __func__);
            return -1;

        case ATH_OTP_FILE:
            qdf_info(KERN_INFO"\n Selecting  OTP binary for CHIP Version %d", target_revision);
            if (target_version == AR6004_REV1_VERSION) {
                filename = AR6004_REV1_OTP_FILE;
            } else if (target_version == AR9888_REV2_VERSION) {
                filename = AR9888_REV2_OTP_FILE;
            } else if (target_version == AR9887_REV1_VERSION) {
                filename = AR9887_REV1_OTP_FILE;
            } else if (target_version == AR9888_DEV_VERSION) {
                filename = AR9888_DEV_OTP_FILE;
            } else if (target_type == TARGET_TYPE_AR900B) {
                if(target_revision == AR900B_REV_1){
                    filename = AR900B_VER1_OTP_FILE;
                } else if(target_revision == AR900B_REV_2) {
                    filename = AR900B_VER2_OTP_FILE;
                }
            } else if (target_type == TARGET_TYPE_QCA9984) {
                if(emu_type == 0) {
                    if(target_version == QCA9984_DEV_VERSION) {
                        filename = QCA9984_HW_VER1_OTP_FILE;
                    }
                } else {
                    /* otp.bin must not be loaded for emulation
                     * platforms and hence ignoring QCA9984_EMU
                     * cases
                     */
                }
            } else if (target_type == TARGET_TYPE_QCA9888) {
                if(emu_type == 0) {
                    if(target_version == QCA9888_DEV_VERSION) {
                        filename = QCA9888_HW_VER2_OTP_FILE;
                    }
                } else {
                    /* otp.bin must not be loaded for emulation
                     * platforms and hence ignoring QCA9984_EMU
                     * cases
                     */
                }
            } else if (target_type == TARGET_TYPE_IPQ4019) {
                if(emu_type == 0) {
                    if(target_version == IPQ4019_DEV_VERSION) {
                        filename = IPQ4019_HW_VER1_OTP_FILE;
                    }
                } else {
                    /* otp.bin must not be loaded for emulation
                     * platforms and hence ignoring IPQ4019_EMU
                     * cases
                     */
                }
            } else {
                qdf_info("%s: no OTP file defined", __func__);
                return -ENOENT;
            }
            if (ol_ath_code_data_swap(soc,filename,ATH_OTP_FILE)) {
                return -EIO;
            }
            break;

        case ATH_FIRMWARE_FILE:
            if (testmode) {
                if ( testmode == 1 ) {
                    A_UINT32 param;

                    qdf_info("%s: Test mode", __func__);
                    if (target_version == AR6004_REV1_VERSION) {
                        filename = AR6004_REV1_UTF_FIRMWARE_FILE;
                    } else if (target_version == AR9888_REV2_VERSION) {
                        filename = AR9888_REV2_UTF_FIRMWARE_FILE;
                    } else if (target_version == AR9887_REV1_VERSION) {
                        filename = AR9887_REV1_UTF_FIRMWARE_FILE;
                    } else if (target_version == AR9888_DEV_VERSION) {
                        filename = AR9888_DEV_UTF_FIRMWARE_FILE;
                    } else if (target_type == TARGET_TYPE_AR900B) {
                        if(target_revision == AR900B_REV_1){
                            filename = AR900B_VER1_UTF_FIRMWARE_FILE;
                        } else if(target_revision == AR900B_REV_2) {
                            filename = AR900B_VER2_UTF_FIRMWARE_FILE;
                        }
                    } else if (target_type == TARGET_TYPE_QCA9984) {
                        if(emu_type == 0) {
                            if(target_version == QCA9984_DEV_VERSION) {
                                filename = QCA9984_HW_VER1_UTF_FIRMWARE_FILE;
                            }
                        } else if(emu_type == 1) {
                            filename = QCA9984_M2M_VER1_UTF_FIRMWARE_FILE;
                        } else if(emu_type == 2) {
                            filename = QCA9984_BB_VER1_UTF_FIRMWARE_FILE;
                        }
                    } else if (target_type == TARGET_TYPE_QCA9888) {
                        if(emu_type == 0) {
                            if(target_version == QCA9888_DEV_VERSION) {
                                filename = QCA9888_HW_VER2_UTF_FIRMWARE_FILE;
                            }
                        } else if(emu_type == 1) {
                            filename = QCA9888_M2M_VER2_UTF_FIRMWARE_FILE;
                        } else if(emu_type == 2) {
                            filename = QCA9888_BB_VER2_UTF_FIRMWARE_FILE;
                        }
                    } else if (target_type == TARGET_TYPE_IPQ4019) {
                        if(emu_type == 0) {
                            if(target_version == IPQ4019_DEV_VERSION) {
                                filename = IPQ4019_HW_VER1_UTF_FIRMWARE_FILE;
                            }
                        } else if(emu_type == 1) {
                            filename = IPQ4019_M2M_VER1_UTF_FIRMWARE_FILE;
                        } else if(emu_type == 2) {
                            filename = IPQ4019_BB_VER1_UTF_FIRMWARE_FILE;
                        }
                    } else {
                        qdf_info("%s: no firmware file defined", __func__);
                        return EOK;
                    }
                    qdf_info("%s: Downloading firmware file: %s", __func__, filename);

                    if (BMIReadMemory(hif_hdl,
                        host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_fw_swap)),
                        (A_UCHAR *)&param,4, bmi_handle)!= A_OK)
                    {
                        qdf_info("BMIReadMemory for setting FW swap flags failed ");
                        return A_ERROR;
                    }
                    param |= HI_DESC_IN_FW_BIT;
                    if (BMIWriteMemory(hif_hdl,
                        host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_fw_swap)),
                        (A_UCHAR *)&param,4, bmi_handle) != A_OK)
                    {
                        qdf_info("BMIWriteMemory for setting FW swap flags failed ");
                        return A_ERROR;
                    }
                }
                if(ol_ath_code_data_swap(soc,filename,ATH_UTF_FIRMWARE_FILE)) {
                    return -EIO;
                }
            }
            else {
                qdf_info(KERN_INFO"\n Mission mode: Firmware CHIP Version %d", target_revision);
                if (target_version == AR6004_REV1_VERSION) {
                    filename = AR6004_REV1_FIRMWARE_FILE;
                } else if (target_version == AR9888_REV2_VERSION) {
#if QCA_LTEU_SUPPORT
                    if (wlan_psoc_nif_feat_cap_get(psoc, WLAN_SOC_F_LTEU_SUPPORT)) {
                        filename = AR9888_REV2_FIRMWARE_FILE_LTEU;
                    } else {
#endif
#if QCA_AIRTIME_FAIRNESS
                    if (target_if_atf_get_mode(psoc)) {
                        filename = AR9888_REV2_ATF_FIRMWARE_FILE;
                    } else {
#endif
                        filename = AR9888_REV2_FIRMWARE_FILE;
#if QCA_AIRTIME_FAIRNESS
                   }
#endif
#if QCA_LTEU_SUPPORT
                   }
#endif
                } else if (target_version == AR9887_REV1_VERSION) {
                    filename = AR9887_REV1_FIRMWARE_FILE;
                } else if (target_version == AR9888_DEV_VERSION) {
                    filename = AR9888_DEV_FIRMWARE_FILE;
                } else if (target_type == TARGET_TYPE_AR900B) {
                    if(target_revision == AR900B_REV_1){
                        filename = AR900B_VER1_FIRMWARE_FILE;
                    } else if(target_revision == AR900B_REV_2) {
                        filename = AR900B_VER2_FIRMWARE_FILE;
                    }
                } else if (target_type == TARGET_TYPE_QCA9984) {
                    if(emu_type == 0) {
                        if(target_version == QCA9984_DEV_VERSION) {
                            filename = QCA9984_HW_VER1_FIRMWARE_FILE;
                        }
                    } else if(emu_type == 1) {
                        filename = QCA9984_M2M_VER1_FIRMWARE_FILE;
                    } else if(emu_type == 2) {
                        filename = QCA9984_BB_VER1_FIRMWARE_FILE;
                    }
                } else if (target_type == TARGET_TYPE_QCA9888) {
                    if(emu_type == 0) {
                        if(target_version == QCA9888_DEV_VERSION) {
                            filename = QCA9888_HW_VER2_FIRMWARE_FILE;
                        }
                    } else if(emu_type == 1) {
                        filename = QCA9888_M2M_VER2_FIRMWARE_FILE;
                    } else if(emu_type == 2) {
                        filename = QCA9888_BB_VER2_FIRMWARE_FILE;
                    }
                } else if (target_type == TARGET_TYPE_IPQ4019) {
                    if(emu_type == 0) {
                        if(target_version == IPQ4019_DEV_VERSION) {
                            filename = IPQ4019_HW_VER1_FIRMWARE_FILE;
                        }
                    } else if(emu_type == 1) {
                        filename = IPQ4019_M2M_VER1_FIRMWARE_FILE;
                    } else if(emu_type == 2) {
                        filename = IPQ4019_BB_VER1_FIRMWARE_FILE;
                    }
                } else {
                    qdf_info("%s: no firmware file defined", __func__);
                    return EOK;
                }
                if(ol_ath_code_data_swap(soc,filename,ATH_FIRMWARE_FILE)) {
                    return -EIO;
                }
                qdf_info("%s: Downloading firmware file: %s", __func__, filename);
            }

#ifdef EPPING_TEST
            if (eppingtest) {
                bypasswmi = TRUE;
                filename = AR6004_REV1_EPPING_FIRMWARE_FILE;
                compressed = 0;
            }
#endif
            break;

        case ATH_PATCH_FILE:
            qdf_info("%s: no Patch file defined", __func__);
            return EOK;

        case ATH_BOARD_DATA_FILE:
            if (target_version == AR6004_REV1_VERSION) {
                filename = AR6004_REV1_BOARD_DATA_FILE;
                qdf_info("%s: Board data file AR6004", __func__);
            } else if (target_version == AR9888_REV2_VERSION) {
                filename = AR9888_REV2_BOARD_DATA_FILE;
                qdf_info("%s: Board data file AR9888v2", __func__);
            } else if (target_version == AR9887_REV1_VERSION) {
                qdf_info("%s: Board data file AR9887v1", __func__);
                if (testmode == 1) {
                    filename = AR9887_REV1_BOARDDATA_FILE;
                } else {
                    filename = AR9887_REV1_BOARD_DATA_FILE;
                }
            } else if (target_version == AR9888_DEV_VERSION) {
                filename = AR9888_DEV_BOARD_DATA_FILE;
                qdf_info("%s: Board data file AR9888", __func__);
            } else if (target_version == SOC_SW_VERSION) {

		/* QDART condition only */
                if(testmode == 1) {

                    /* initialize file vars */
                    qdf_mem_set(buf, MAX_FILENAME_BUFF_SIZE, 0);
                    qdf_mem_set(absolute_filename,MAX_ABSOLUTE_FILE_SIZE, 0);

                    /* get the PATH variable */
                    if (target_type == TARGET_TYPE_AR900B) {
                        if (target_revision == AR900B_REV_1) {
                            pathname = AR900B_VER1_PATH;
                        } else if (target_revision == AR900B_REV_2) {
                            pathname = AR900B_VER2_PATH;
                        }
                    } else if (target_type == TARGET_TYPE_QCA9984) {
                        if(emu_type == 0) {
                            if(target_version == QCA9984_DEV_VERSION) {
                                pathname = QCA9984_HW_VER1_PATH;
                            }
                        } else if(emu_type == 1) {
                            pathname = QCA9984_M2M_VER1_PATH;
                        } else if(emu_type == 2) {
                            pathname = QCA9984_BB_VER1_PATH;
                        }
                    } else if (target_type == TARGET_TYPE_QCA9888) {
                        if(emu_type == 0) {
                            if(target_version == QCA9888_DEV_VERSION) {
                                pathname = QCA9888_HW_VER2_PATH;
                            }
                        } else if(emu_type == 1) {
                            pathname = QCA9888_M2M_VER2_PATH;
                        } else if(emu_type == 2) {
                            pathname = QCA9888_BB_VER2_PATH;
                        }
                    } else if (target_type == TARGET_TYPE_IPQ4019) {
                        if(emu_type == 0) {
                            if(target_version == IPQ4019_DEV_VERSION) {
                                pathname = IPQ4019_HW_VER1_PATH;
                            }
                        } else if(emu_type == 1) {
                            pathname = IPQ4019_M2M_VER1_PATH;
                        } else if(emu_type == 2) {
                            pathname = IPQ4019_BB_VER1_PATH;
                        }
                    }

                    /* get the file name */
                    if(soc->soc_idx == 0){
                        filename = "boarddata_0.bin";
                        qdf_info("\n wifi0 Select filename %s", filename);
                    } else if (soc->soc_idx == 1){
                        filename = "boarddata_1.bin";
                        qdf_info("\n wifi1 Select filename %s", filename);
                    } else {
                        filename = "boarddata_2.bin";
                        qdf_info("\n wifi2 Select filename %s", filename);
                    }


                    /* validate PATH and FILE vars */
                    if ((pathname == NULL) || (filename == NULL)) {
                        qdf_info("\n %s : Unable to get the PATH/FILE name, check chip revision", __FUNCTION__);
                        return EOK;
                    }

                    /* create absolute PATH variable */
                    if (strlcat(absolute_filename, pathname, MAX_ABSOLUTE_FILE_SIZE) >= MAX_ABSOLUTE_FILE_SIZE) {
                        qdf_info("ifname too long: %s", pathname);
                        return -1;
                    }
                    if (strlcat(absolute_filename, filename, MAX_ABSOLUTE_FILE_SIZE) >= MAX_ABSOLUTE_FILE_SIZE) {
                        qdf_info("ifname too long: %s", pathname);
                        return -1;
                    }
                    filename = absolute_filename;

                    qdf_info("\n %s : (Test Mode) For interface (%s) selected filename %s",
                            __FUNCTION__, soc->sc_osdev->netdev->name, absolute_filename);

                } else { /* Mission-mode condition only */
#ifdef CONFIG_AR900B_SUPPORT
                    filename = NULL;
                    qdf_mem_set(buf, MAX_FILENAME_BUFF_SIZE, 0);
                    qdf_mem_set(absolute_filename,MAX_ABSOLUTE_FILE_SIZE, 0);
                    if(ol_get_board_id(soc, buf) < 0) {
                        qdf_info("\n %s : BoardData Download Failed",__FUNCTION__);
                        return -1;
                    }
                    if (target_type == TARGET_TYPE_AR900B) {
                        if(target_revision == AR900B_REV_1){
                            filename = AR900B_VER1_PATH;
                        } else if(target_revision == AR900B_REV_2) {
                            filename = AR900B_VER2_PATH;
                        }
                    } else if (target_type == TARGET_TYPE_QCA9984) {
                        if(emu_type == 0) {
                            if(target_version == QCA9984_DEV_VERSION) {
                                filename = QCA9984_HW_VER1_PATH;
                            }
                        } else if(emu_type == 1) {
                            filename = QCA9984_M2M_VER1_PATH;
                        } else if(emu_type == 2) {
                            filename = QCA9984_BB_VER1_PATH;
                        }
                    } else if (target_type == TARGET_TYPE_QCA9888) {
                        if(emu_type == 0) {
                            if(target_version == QCA9888_DEV_VERSION) {
                                filename = QCA9888_HW_VER2_PATH;
                            }
                        } else if(emu_type == 1) {
                            filename = QCA9888_M2M_VER2_PATH;
                        } else if(emu_type == 2) {
                            filename = QCA9888_BB_VER2_PATH;
                        }
                    } else if (target_type == TARGET_TYPE_IPQ4019) {
                        if(emu_type == 0) {
                            if(target_version == IPQ4019_DEV_VERSION) {
                                filename = IPQ4019_HW_VER1_PATH;
                            }
                        } else if(emu_type == 1) {
                            filename = IPQ4019_M2M_VER1_PATH;
                        } else if(emu_type == 2) {
                            filename = IPQ4019_BB_VER1_PATH;
                        }
                    } else {
                        qdf_info("\n %s : Unable to get the PATH/FILE name, check chip revision", __FUNCTION__);
                        return EOK;
                    }

                    /* validate PATH and FILE vars */
                    if (filename == NULL) {
                        qdf_info("\n %s : Unable to get the FILE name, check chip revision", __FUNCTION__);
                        return EOK;
                    }

                    if (strlcat(absolute_filename,filename,MAX_ABSOLUTE_FILE_SIZE) >= MAX_ABSOLUTE_FILE_SIZE) {
                        qdf_info("filename too long: %s", filename);
                        return -1;
                    }
                    if (strlcat(absolute_filename,buf,MAX_ABSOLUTE_FILE_SIZE) >= MAX_ABSOLUTE_FILE_SIZE) {
                        qdf_info("filename too long: %s", filename);
                        return -1;
                    }
                    filename = absolute_filename ;
#else
                    filename = ARXXXX_DEV_BOARD_DATA_FILE;
#endif
                }
            } else {
                qdf_info("%s: no Board data file defined", __func__);
                return EOK;
            }
            qdf_info("%s: Board Data File download to address=0x%x file name=%s", __func__,
                     address,(filename ? filename : "ERROR: NULL FILE NAME"));
            break;

        case ATH_FLASH_FILE:
                qdf_info("%s: flash data file defined", __func__);
            break;

    }
    // No File present for Flash only memmapped
    if(file != ATH_FLASH_FILE) {
        if (!filename) {
            qdf_info("%s:%d filename null ", __func__, __LINE__);
            if ( emu_type && file == ATH_OTP_FILE ) {
                return -ENOENT;
            }
            return -1;
        }

        rfwst = ol_ath_request_firmware((struct firmware_priv **) &fw_entry,
                filename, soc->sc_osdev->device, soc->device_id);

	if ((rfwst != 0)) {
            qdf_info("%s: Failed to get %s", __func__, filename);
            if (file == ATH_OTP_FILE)
                return -ESKIP_OTP;

            return -ENOENT;
        }

        srcp = (uint32_t *)fw_entry->data;
        orig_size = fw_entry->size;
        fw_entry_size = fw_entry->size = (orig_size + (sizeof(int) - 1)) & ~(sizeof(int) - 1); /* round off to 4 bytes */
        qdf_info(KERN_INFO"%s %d: downloading file %d, Download data len %zd", __func__, __LINE__, file, fw_entry->size);
    }

    if (file == ATH_FLASH_FILE)
    {
#ifdef  ATH_CAL_NAND_FLASH
        int ret_val=0;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,4,103)
        int ret_len;
#else
        char * filename;
#endif
	u_int32_t cal_location;
#endif
#ifdef AH_CAL_IN_FLASH_PCI
        if (!soc->cal_in_flash) {
            qdf_info("%s: flash cal data address is not mapped", __func__);
            return -EINVAL;
        }

#ifdef  ATH_CAL_NAND_FLASH
			cal_location = CalAddr[scn->cal_idx-1];
			QDF_PRINT_INFO(ic->ic_print_idx, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Cal location [%d]: %08x\n", scn->cal_idx-1, cal_location);
			ptr = vmalloc(QC98XX_EEPROM_SIZE_LARGEST);
        if ( NULL == ptr )
        {
            qdf_info("%s %d: flash cal data(NAND)memory allocation failed",__func__, __LINE__);
            return -EINVAL;
        }
        else
        {
            if(soc->soc_idx == 0)
            {
                qdf_info("\n Wifi0 NAND FLASH Select OFFSET 0x%x",cal_location + FLASH_CAL_START_OFFSET);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,4,103)
                ret_val = OS_NAND_FLASH_READ(ATH_CAL_NAND_PARTITION, cal_location + FLASH_CAL_START_OFFSET ,QC98XX_EEPROM_SIZE_LARGEST,&ret_len,ptr);
#else
                filename = CALDATA0_FILE_PATH;
                if(A_ERROR ==  qdf_fs_read(filename, 0, QC98XX_EEPROM_SIZE_LARGEST, ptr)) {
                        qdf_info("%s[%d], Error: Reading %s failed.", __func__, __LINE__, filename);
                        ret_val =  -EINVAL;
                }
#endif
            }
            else
            {
                qdf_info("\n %s NAND FLASH Select OFFSET 0x%x",soc->sc_osdev->netdev->name, cal_location + FLASH_CAL_START_OFFSET);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,4,103)
                ret_val = OS_NAND_FLASH_READ(ATH_CAL_NAND_PARTITION, cal_location + FLASH_CAL_START_OFFSET ,QC98XX_EEPROM_SIZE_LARGEST,&ret_len,ptr);
#else
                if (soc->soc_idx == 1) {
                        filename = CALDATA1_FILE_PATH;
                } else if (soc->soc_idx == 2) {
                        filename = CALDATA2_FILE_PATH;
                }
                if(A_ERROR ==  qdf_fs_read(filename, 0, QC98XX_EEPROM_SIZE_LARGEST, ptr)) {
                        qdf_info("%s[%d], Error: Reading %s failed.", __func__, __LINE__, filename);
                        ret_val =  -EINVAL;
                }
#endif
            }
            if (ret_val)
            {
                qdf_info("%s %d: ATH_CAL_NAND Partition flash cal data(NAND) read failed",__func__, __LINE__ );
                if (ptr) {
                    vfree(ptr);
                }
                return -EINVAL;
            }
        }
#else
        if (!soc->cal_mem ){
            qdf_info("%s: NOR FLASH cal data address is not mapped", __func__);
            return -EINVAL;
        }

        if(soc->soc_idx == 0)
        {
            qdf_info("\n NOR FLASH Wifi0 Select OFFSET %x",FLASH_CAL_START_OFFSET);
            ptr = soc->cal_mem + FLASH_CAL_START_OFFSET;
        }
        else
        {
            qdf_info("\n %s NOR FLASH Select OFFSET %x",soc->sc_osdev->netdev->name, FLASH_CAL_START_OFFSET);
            ptr = soc->cal_mem + FLASH_CAL_START_OFFSET;
        }

#endif
#endif

	{
		char * filename;

		if (!soc->cal_in_file) {
			qdf_info("%s: cal data file is not ready.", __func__);
			return -EINVAL;
		}

		if (soc->soc_idx == 0)
		{
			filename = CALDATA0_FILE_PATH;
		}
		else if (soc->soc_idx == 1)
		{
			filename = CALDATA1_FILE_PATH;
		}
		else if (soc->soc_idx == 2)
		{
			filename = CALDATA2_FILE_PATH;
		}
		else
		{
			qdf_info("%s[%d], Error, Please check why none of wifi0 wifi1 or wifi2 is your device name (%s).",
					__func__, __LINE__, soc->sc_osdev->netdev->name);
			return A_ERROR;
		}

		qdf_info("%s[%d] Get Caldata for %s.", __func__, __LINE__, soc->sc_osdev->netdev->name);

		if(NULL == filename)
		{
			qdf_info("%s[%d], Error: File name is null, please assign right caldata file name.", __func__, __LINE__);
			return -EINVAL;
		}

		ptr = vmalloc(QC98XX_EEPROM_SIZE_LARGEST);
		if ( NULL == ptr ){
			qdf_info("%s %d: Memory allocation for calibration file failed.",__func__, __LINE__);
			return -A_NO_MEMORY;
		}

		if(A_ERROR ==  qdf_fs_read(filename, 0, QC98XX_EEPROM_SIZE_LARGEST, ptr))
		{
			soc->cal_in_file = 0;
			qdf_info("%s[%d], Error: Reading %s failed.", __func__, __LINE__, filename);
			if(ptr)
				vfree(ptr);
			return A_ERROR;
		}
	}

        if (NULL == ptr) {
            qdf_info("%s %d: Memory allocation failed.",__func__, __LINE__);

            return -EINVAL;
        }

        if (le16_to_cpu(*(uint16_t *)ptr) != QC98XX_EEPROM_SIZE_LARGEST) {
            soc->cal_in_file = 0;
            qdf_info("%s: flash cal data len %d doesn't equal to %d", __func__, le16_to_cpu(*(uint16_t *)ptr), QC98XX_EEPROM_SIZE_LARGEST);
#if defined(ATH_CAL_NAND_FLASH)
            if (ptr) {
                vfree(ptr);
            }
#endif
            return -EINVAL;
        }

        if (qc98xx_verify_checksum(ptr)){
            soc->cal_in_file = 0;
#if defined(ATH_CAL_NAND_FLASH)
            if (ptr) {
                vfree(ptr);
            }
#endif
            qdf_info("\n ===> CAL CHECKSUM FAILED <=== ");
            return -EINVAL;
        }

        srcp = (uint32_t *)ptr;
        orig_size = QC98XX_EEPROM_SIZE_LARGEST;
        fw_entry_size = (orig_size + 3) & ~3;
        qdf_info("%s %d: Download Flash data len %zd", __func__, __LINE__, fw_entry_size);
    }

    if ( fw_entry && fw_entry->data == NULL) {
        qdf_info("%s:%d fw_entry->data == NULL ", __func__, __LINE__);
#if defined(ATH_CAL_NAND_FLASH) || defined(AH_CAL_IN_FILE_HOST)
        if (ptr) {
            vfree(ptr);
        }
#endif
        return -EINVAL;
    }

    savedestp = destp = vmalloc(fw_entry_size);
    if(destp == NULL)
    {
        qdf_info("%s %d: memory allocation failed",__func__, __LINE__);
	if(file != ATH_FLASH_FILE) {
	    ol_ath_release_firmware(fw_entry);
	}
#if defined(ATH_CAL_NAND_FLASH)
        if (ptr) {
            vfree(ptr);
        }
#endif
        return A_ERROR;
    }
    pad_dst = (uint8_t *)destp;
    pad_src = (uint8_t *)srcp;

    /* Add pad bytes if required */
    for (i = 0; i < fw_entry_size; i++) {
        if (i < orig_size) {
            pad_dst[i] = pad_src[i];
        } else {
            pad_dst[i] = 0;
        }
    }

    for (i = 0; i < (fw_entry_size)/4; i++) {
	    *destp = qdf_cpu_to_le32(*srcp);
	    destp++; srcp++;
    }

    destp = savedestp;

    tempEeprom = NULL;

    if ((file == ATH_BOARD_DATA_FILE) || (file == ATH_FLASH_FILE))
    {
        u_int32_t board_ext_address = 0;
        int32_t board_ext_data_size;

        tempEeprom = OS_MALLOC(soc->sc_osdev, fw_entry_size, GFP_ATOMIC);
        if (!tempEeprom) {
            qdf_info("%s: Memory allocation failed", __func__);
	    if(file != ATH_FLASH_FILE) {
		ol_ath_release_firmware(fw_entry);
	    }
#if defined(ATH_CAL_NAND_FLASH)
            if (ptr) {
                vfree(ptr);
            }
#endif
            if(destp) {
                vfree(destp);
            }

            return A_ERROR;
        }

        OS_MEMCPY(tempEeprom, (u_int8_t *)destp, fw_entry_size);

        switch (target_type)
        {
            default:
                board_ext_data_size = 0;
                break;
            case TARGET_TYPE_AR6004:
                board_data_size =  AR6004_BOARD_DATA_SZ;
                board_ext_data_size = AR6004_BOARD_EXT_DATA_SZ;
                break;
            case TARGET_TYPE_AR9888:
                board_data_size =  AR9888_BOARD_DATA_SZ;
                board_ext_data_size = AR9888_BOARD_EXT_DATA_SZ;
                break;
        }

#ifdef SOFTMAC_FILE_USED
        ar6000_softmac_update(ar, tempEeprom, board_data_size);
#endif

        /* Determine where in Target RAM to write Board Data */
        if (BMIReadMemory(hif_hdl,
                HOST_INTEREST_ITEM_ADDRESS(target_type, hi_board_ext_data),
                (u_int8_t *)&board_ext_address, 4, bmi_handle)!= A_OK)
        {
            qdf_info("BMIReadMemory for Board extended Data download failed ");
	    if(file != ATH_FLASH_FILE) {
		ol_ath_release_firmware(fw_entry);
	    }
#if defined(ATH_CAL_NAND_FLASH)
            if (ptr) {
                vfree(ptr);
            }
#endif
            if(destp) {
                vfree(destp);
            }

            return A_ERROR;
        }
        qdf_info(KERN_INFO"Board extended Data download address: 0x%x", board_ext_address);

        /*
         * Check whether the target has allocated memory for extended board
         * data and file contains extended board data
         */
        if ((board_ext_address) && (fw_entry_size == (board_data_size + board_ext_data_size)))
        {
            u_int32_t param;

            status = BMIWriteMemory(hif_hdl, board_ext_address,
                    (u_int8_t *)(((uintptr_t)tempEeprom) + board_data_size), board_ext_data_size, bmi_handle);

            if (status != EOK) {
                qdf_info("%s: BMI operation failed: %d", __func__, __LINE__);
		if(file != ATH_FLASH_FILE) {
		    ol_ath_release_firmware(fw_entry);
		}
#if defined(ATH_CAL_NAND_FLASH)
                if (ptr) {
                    vfree(ptr);
                }
#endif
                if(destp) {
                    vfree(destp);
                }

                return -1;
            }

            /* Record the fact that extended board Data IS initialized */
            param = (board_ext_data_size << 16) | 1;
            BMIWriteMemory(hif_hdl,
                    HOST_INTEREST_ITEM_ADDRESS(target_type, hi_board_ext_data_config),
                    (u_int8_t *)&param, 4, bmi_handle);

            fw_entry_size = board_data_size;
        }
    }

    offset = 0;

#if defined(ATH_CONFIG_FW_SIGN)

    A_UINT32 length;
    SIGN_HEADER_T *sign_header;

    if (0 || (file == ATH_FIRMWARE_FILE) || (file == ATH_OTP_FILE) || (file == ATH_BOARD_DATA_FILE)) {

        if (file == ATH_BOARD_DATA_FILE) {
            /* board data is not uploaded using segmented write but
             * sending to BMI_SEGMENTED_WRITE_ADDR is the only way to
             * reset the secure boot state machine. Hence the following.
             */
		#waring "ATH_BOARD_DATA_FILE inside\n";
            status = BMISignStreamStart(hif_hdl, BMI_SEGMENTED_WRITE_ADDR, (u_int8_t *)destp, sizeof(SIGN_HEADER_T), bmi_handle);
        } else {
            status = BMISignStreamStart(hif_hdl, address, (u_int8_t *)destp, sizeof(SIGN_HEADER_T), bmi_handle);
        }

        if(A_FAILED(status)) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to start sign stream\n"));
#if defined(ATH_CAL_NAND_FLASH)
            if (ptr) {
                vfree(ptr);
            }
#endif
            if(destp) {
                vfree(destp);
            }

            return A_ERROR;
        }

        sign_header = (SIGN_HEADER_T *)(destp);
        offset = sizeof(SIGN_HEADER_T);
        fw_entry_size = length = sign_header->rampatch_len - sizeof(SIGN_HEADER_T);
    }

#endif

    if (compressed) {

        status = BMIFastDownload(hif_hdl, address, (u_int8_t *)destp + offset, fw_entry_size, bmi_handle);
    } else {
        status = (0
#if ! defined(ATH_CONFIG_FW_SIGN)
            || (file == ATH_BOARD_DATA_FILE)
#endif
            || (file == ATH_FLASH_FILE)
            );
        if (status && tempEeprom) {
            status = BMIWriteMemory(hif_hdl, address, (u_int8_t *)tempEeprom, fw_entry_size, bmi_handle);
        } else {

#if defined(ATH_CONFIG_FW_SIGN)
            status = BMIWriteMemory(hif_hdl, address, (u_int8_t *)destp + offset, fw_entry_size, bmi_handle);
#else
            status = BMIWriteMemory(hif_hdl, address, (u_int8_t *)destp, fw_entry_size, bmi_handle);
#endif
        }
    }

#if defined(ATH_CONFIG_FW_SIGN)
    if (0 || (file == ATH_FIRMWARE_FILE) || (file == ATH_OTP_FILE) || (file == ATH_BOARD_DATA_FILE)) {

        status = BMISignStreamStart(hif_hdl, 0, (u_int8_t *)destp + length + sizeof(SIGN_HEADER_T), sign_header->total_len - sign_header->rampatch_len, bmi_handle);

        if(A_FAILED(status)) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to end sign stream\n"));
        }
    }
#endif

    if (file == ATH_FLASH_FILE) {
#if defined(ATH_CAL_NAND_FLASH)
        if (ptr) {
            vfree(ptr);
        }
#endif
    }

    if(destp) {
        vfree(destp);
    }

    if (tempEeprom) {
        OS_FREE(tempEeprom);
    }

    if (status != EOK) {
        qdf_info("BMI operation failed: %d", __LINE__);
	if(file != ATH_FLASH_FILE) {
	    ol_ath_release_firmware(fw_entry);
	}
        return -1;
    }

    if(file != ATH_FLASH_FILE) {
	ol_ath_release_firmware(fw_entry);
    }

    return status;
}

static inline int
ol_download_cal_data (ol_ath_soc_softc_t  *soc,
                      u_int32_t address, bool compressed)
{
    int status = -EINVAL;
    int file = -EINVAL;

        if ( soc->cal_in_file )
    {
        /*
         * FLASH has highest priority so try to download it first.
         */
        status = ol_transfer_bin_file(soc, ATH_FLASH_FILE, address, FALSE);
        file = ATH_FLASH_FILE;
    }
    if (status) {
        /*
         * EEPROM has second highest priority so try to download it now.
         */
        status = ol_transfer_target_eeprom_caldata(soc, address, FALSE);
        file = ATH_TARGET_EEPROM_FILE;
        if (status) {
            /*
             * Board data has least priority so try to download it now.
             */
            status = ol_transfer_bin_file(soc, ATH_BOARD_DATA_FILE, address, FALSE);
            file = ATH_BOARD_DATA_FILE;
        }
    }
    if (status) {
        qdf_info("%s: Board data download failed, download address: %pK",
                 __func__, (void *)(uintptr_t)address);
    } else {
        qdf_info("%s: Board data file: %d successfully downloaded, download address: %pK",
                 __func__, file, (void *)(uintptr_t)address);
    }
    return status;
}

int
ol_ath_download_firmware(ol_ath_soc_softc_t *soc)
{
    u_int32_t param = 0, address = 0, flash_download_fail = 0;
    int status = !EOK;
    int phase = 0 ;
    uint32_t target_type;
    uint32_t target_version;
    void *hif_hdl;
    struct bmi_info *bmi_handle;

    hif_hdl = lmac_get_hif_hdl(soc->psoc_obj);
    if (!hif_hdl)
    {
	    qdf_info("Failed to get valid hif_hdl ");
	    goto failed;
    }
    target_type = lmac_get_tgt_type(soc->psoc_obj);
    target_version = lmac_get_tgt_version(soc->psoc_obj);
    bmi_handle = soc->bmi_handle;

    if (soc->device_id != AR9887_DEVICE_ID && target_type != TARGET_TYPE_AR9888) {
        if( soc->cal_in_file )
        {
            /* Transfer Board Data from Target EEPROM to Target RAM */
            /* Determine where in Target RAM to write Board Data */
            if (target_version != AR6004_VERSION_REV1_3) {
                BMIReadMemory(hif_hdl,
                        host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_board_data)),
                        (u_int8_t *)&address, 4, bmi_handle);
            }

            qdf_info("\n Target Version is %x",target_version);
            if (!address) {
                if (target_version == AR6004_REV1_VERSION)  {
                    address = AR6004_REV1_BOARD_DATA_ADDRESS;
                } else if (target_version == AR6004_VERSION_REV1_3) {
                    address = AR6004_REV5_BOARD_DATA_ADDRESS;
                }
                qdf_info("%s: Target address not known! Using 0x%x", __func__, address);
            }
            /* Write EEPROM or Flash data to Target RAM */
            qdf_info("\n Flash Download Address  %x ",address);
            status = ol_transfer_bin_file(soc, ATH_FLASH_FILE, address, FALSE);
            if(status != EOK) {
                flash_download_fail = 1;
            }

            /* Record the fact that Board Data is initialized */
            if ((!flash_download_fail) && (target_version != AR6004_VERSION_REV1_3)) {
                qdf_info("\n Board data initialized");
                param = 1;
                BMIWriteMemory(hif_hdl,
                        host_interest_item_address(target_type,
                            offsetof(struct host_interest_s, hi_board_data_initialized)),
                        (u_int8_t *)&param, 4, bmi_handle);
            }
        } /*flash mode */

        if (soc->recovery_in_progress == 0 || target_type == TARGET_TYPE_IPQ4019)
        {
            /*
             * Download first otp bin to get a board id
             */
            /* Transfer One Time Programmable data */
            qdf_info(KERN_INFO"%s: Download OTP, flash download ADDRESS 0x%x",__func__,address);
            address = BMI_SEGMENTED_WRITE_ADDR;
#if defined(ATH_CONFIG_FW_SIGN)
            status = ol_transfer_bin_file(soc, ATH_OTP_FILE, address, FALSE);
#else
            status = ol_transfer_bin_file(soc, ATH_OTP_FILE, address, TRUE);
#endif
            if (status == EOK) {
                /* Execute the OTP code only if entry found and downloaded */
                /* FLASH MODE and FILE MODE ONLY */
                if ( soc->cal_in_file )
                {
                    if(flash_download_fail) {
                        param = PARAM_EEPROM_SECTION_MAC; /* Get MAC address only from Eeprom */
                    }
                    else {
                        param = PARAM_GET_BID_FROM_FLASH; /* Get BoardID first */
                    }
                }// soc->cal_in_flash and soc->cal_in_file
                else {
                    param = PARAM_GET_CHIPVER_BID; /* Get BoardID first */
                }
                if ((1 << soc->soc_idx) && ol_dual_band_5g_radios) {
                    qdf_info("\n Dual-band configuring with 2.4Ghz");
                    param |= PARAM_DUAL_BAND_5G;
                } else if ((1 << soc->soc_idx) && ol_dual_band_2g_radios) {
                    qdf_info("\n Dual-band configuring with 5Ghz");
                    param |= PARAM_DUAL_BAND_2G;
                }
                qdf_info(KERN_INFO"\n First OTP send param %x", param);
                if((status = BMIExecute(hif_hdl, address, &param, bmi_handle, 1))!= A_OK ) {
                    qdf_info("%s : OTP download and Execute failed . status :%d  ",__func__,status);
                    phase = 2 ;
                    goto failed;
                }
                qdf_info("%s :First OTP download and Execute is good address:0x%x return param %d",__func__, param,address);
            } else if ( status == -1 ) {
                qdf_info("%s : ol_transfer_bin_file failed. status :%d ",__func__,status);
                return status ;
            }

#ifdef CONFIG_AR900B_SUPPORT
#define RET_BOARDID_LSB      10
#define RET_BOARDID_MASK     0x1f
        board_id = (param >> RET_BOARDID_LSB) & RET_BOARDID_MASK;
        soc->board_id = board_id;
        if(
        soc->cal_in_file &&
        flash_download_fail) {
            board_id = 0;
        }
#define RET_CHIPID_LSB      15
#define RET_CHIPID_MASK     0x03
        chipid = (param >> RET_CHIPID_LSB) & RET_CHIPID_MASK;
        soc->chip_id = chipid;
        qdf_info("%s:##Board Id %d , CHIP Id %d",__func__,board_id, chipid);
#endif
        if (status == -ESKIP_OTP) {
            /* A specific halphy test needs wifi load to be successful
               without downloading the OTP.bin. Allow load by skipping OTP
               download failure */
            soc->chip_id = chipid = lmac_get_tgt_revision(soc->psoc_obj);
            qdf_info("WARNING!!!!: OTP download failed. chip id assigned with [%x]\n",chipid);
        }

        } else {
            /* FW Recovery in progress, use the already stored board id */
#ifdef CONFIG_AR900B_SUPPORT
            board_id = soc->board_id;
            if(
               soc->cal_in_file &&
               flash_download_fail) {
                board_id = 0;
            }
            chipid = soc->chip_id;
            QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO,
                           "%s:##Recovered Board Id %d , CHIP Id %d\n",__func__,board_id, chipid);
#endif
        }
    }

    /* Transfer Board Data from Target EEPROM to Target RAM */
    /* Determine where in Target RAM to write Board Data */
    if (target_version != AR6004_VERSION_REV1_3) {
         BMIReadMemory(hif_hdl,
         host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_board_data)),
                                   (u_int8_t *)&address, 4, bmi_handle);
    }

    if (!address) {
         if (target_version == AR6004_REV1_VERSION)  {
             address = AR6004_REV1_BOARD_DATA_ADDRESS;
         } else if (target_version == AR6004_VERSION_REV1_3) {
              address = AR6004_REV5_BOARD_DATA_ADDRESS;
         }
        qdf_info("%s: Target address not known! Using 0x%x", __func__, address);
    }

    if(soc->device_id == AR9887_DEVICE_ID) {
         status = ol_download_cal_data(soc, address, FALSE);
    } else if ( soc->device_id == AR9888_DEVICE_ID ) {
        if ( soc->cal_in_file )
        {
            /* Write EEPROM or Flash data to Target RAM */
            status = ol_transfer_bin_file(soc, ATH_FLASH_FILE, address, FALSE);
        }
    }

    if (!lmac_is_target_ar900b(soc->psoc_obj)) {
        param = 0;
        board_id = 1;
    }

    if ((status == EOK && param == 0) && (board_id !=0 )) {
        /* Record the fact that Board Data is initialized */
        if (target_version != AR6004_VERSION_REV1_3) {
            param = 1;
            BMIWriteMemory(hif_hdl,
                           host_interest_item_address(target_type,
                               offsetof(struct host_interest_s, hi_board_data_initialized)),
                           (u_int8_t *)&param, 4, bmi_handle);
        }
    } else {
        qdf_info(KERN_INFO"%s: BOARDDATA DOWNLOAD TO address 0x%x",__func__, address);
        /* Flash is either not available or invalid */
        if ((status = ol_transfer_bin_file(soc, ATH_BOARD_DATA_FILE, address, FALSE)) != EOK) {
            phase = 1 ;
            goto failed;
        }

        /* Record the fact that Board Data is initialized */
        if (target_version != AR6004_VERSION_REV1_3) {
            param = 1;
            BMIWriteMemory(hif_hdl,
                           host_interest_item_address(target_type,
                               offsetof(struct host_interest_s, hi_board_data_initialized)),
                           (u_int8_t *)&param, 4, bmi_handle);
        }

        /* Transfer One Time Programmable data */
        address = BMI_SEGMENTED_WRITE_ADDR;
        qdf_info(KERN_INFO"%s: Using 0x%x for the remainder of init", __func__, address);
#if defined(ATH_CONFIG_FW_SIGN)
        status = ol_transfer_bin_file(soc, ATH_OTP_FILE, address, FALSE);
#else
        status = ol_transfer_bin_file(soc, ATH_OTP_FILE, address, TRUE);
#endif
        if (status == EOK) {
            /* Execute the OTP code only if entry found and downloaded */
            if ( soc->cal_in_file )
            {
                if(flash_download_fail) {
                    param = PARAM_EEPROM_SECTION_MAC; /* Get MAC address only from Eeprom */
                }
                else {
                    if(ol_validate_otp_mod_param(
                           cfg_get(soc->psoc_obj, CFG_OL_OTP_MOD_PARAM))< 0 ) {
                           qdf_info("\n [Flash] : Ignore Module param");
                           param = PARAM_FLASH_SECTION_ALL;
                    }
                    else{
                        param = cfg_get(soc->psoc_obj, CFG_OL_OTP_MOD_PARAM);
                        qdf_info("\n [Flash] : Module param 0x%x selected", 
                                cfg_get(soc->psoc_obj, CFG_OL_OTP_MOD_PARAM));
                    }
                }
            } // soc->cal_in_flash and soc->cal_in_file
            else {
                if(ol_validate_otp_mod_param(
                         cfg_get(soc->psoc_obj, CFG_OL_OTP_MOD_PARAM)) < 0) {
                    qdf_info(KERN_INFO"\n [Non-Flash] : Ignore Module param");
                    param = PARAM_EEPROM_SECTION_MAC | PARAM_EEPROM_SECTION_REGDMN | PARAM_EEPROM_SECTION_CAL;
                }
                else {
                    param = cfg_get(soc->psoc_obj, CFG_OL_OTP_MOD_PARAM);
                    qdf_info("\n [Non-Flash] mode :  Module param 0x%x selected", 
                            cfg_get(soc->psoc_obj, CFG_OL_OTP_MOD_PARAM));
                }
            }

            if ((1 << soc->soc_idx) && ol_dual_band_5g_radios) {
                qdf_info("\n Dual-band configuring with 2.4Ghz");
                param |= PARAM_DUAL_BAND_5G;
            } else if ((1 << soc->soc_idx) && ol_dual_band_2g_radios) {
                qdf_info("\n Dual-band configuring with 5Ghz");
                param |= PARAM_DUAL_BAND_2G;
            }

            qdf_info(KERN_INFO"\n Second otp download Param %x ", param);
            if((status = BMIExecute(hif_hdl, address, &param, bmi_handle, 1))!= A_OK ) {
                qdf_info("%s : OTP download and Execute failed . status :%d  ",__func__,status);
                phase = 2 ;
                goto failed;
            }
            qdf_info("%s : Second OTP download and Execute is good, param=0x%x ",__func__,param);
        } else if ( status == -1 ) {
            qdf_info("%s : ol_transfer_bin_file failed. status :%d ",__func__,status);
            return status ;
        }
    }

    /* Bypass PLL setting */
    if (target_version == AR9888_REV2_VERSION || target_version == AR9887_REV1_VERSION) {
        param = 1;
        BMIWriteMemory(hif_hdl,host_interest_item_address(target_type,
                        offsetof(struct host_interest_s, hi_skip_clock_init)),(u_int8_t *)&param, 4, bmi_handle);
    }

    /* Download Target firmware - TODO point to target specific files in runtime */
    address = BMI_SEGMENTED_WRITE_ADDR;
#if defined(ATH_CONFIG_FW_SIGN)
    if((status = ol_transfer_bin_file(soc, ATH_FIRMWARE_FILE, address, FALSE)) != EOK) {
#else
    if ((status = ol_transfer_bin_file(soc, ATH_FIRMWARE_FILE, address, TRUE)) != EOK) {
#endif
            phase = 3 ;
            goto failed;
    }

    /* Apply the patches */
    if (ol_check_dataset_patch(soc, &address))
    {
        if ((status = ol_transfer_bin_file(soc, ATH_PATCH_FILE, address, FALSE)) != EOK) {
            phase = 4 ;
            goto failed;
        }
        BMIWriteMemory(hif_hdl,
                     host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_dset_list_head)),
                     (u_int8_t *)&address, 4, bmi_handle);
    }

    if (soc->enableuartprint) {
        /* Configure GPIO AR9888 UART */
        if (target_version == AR6004_VERSION_REV1_3) {
            param = 15;
        } else {
            param = 7;
        }
        BMIWriteMemory(hif_hdl,
                host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_dbg_uart_txpin)),
                (u_int8_t *)&param, 4, bmi_handle);
        param = 1;
        BMIWriteMemory(hif_hdl,
                host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_serial_enable)),
                (u_int8_t *)&param, 4, bmi_handle);

        /* band rates is 19200 for AR9888v2 */
        if (target_version == AR9888_REV2_VERSION || target_version == AR9887_REV1_VERSION) {
            param = 19200;
            BMIWriteMemory(hif_hdl,
                    host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_desired_baud_rate)),
                    (u_int8_t *)&param, 4, bmi_handle);
        }
    }else {
        /*
         * Explicitly setting UART prints to zero as target turns it on
         * based on scratch registers.
         */
        param = 0;
        BMIWriteMemory(hif_hdl,
                host_interest_item_address(target_type, offsetof(struct host_interest_s,hi_serial_enable)),
                (u_int8_t *)&param, 4, bmi_handle);
    }
    if (cfg_get(soc->psoc_obj, CFG_OL_ALLOCRAM_TRACK_MAX) > 0) {
        param = cfg_get(soc->psoc_obj, CFG_OL_ALLOCRAM_TRACK_MAX);
        BMIWriteMemory(hif_hdl,
                host_interest_item_address(target_type,
                    offsetof(struct host_interest_s, hi_allocram_track_max)),
                    (u_int8_t *)&param, 4, bmi_handle);
    }
    if (target_version == AR6004_VERSION_REV1_3) {
        A_UINT32 blocksizes[HTC_MAILBOX_NUM_MAX] = {0x10,0x10,0x10,0x10};
        BMIWriteMemory(hif_hdl,
                host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_mbox_io_block_sz)),
                 (A_UCHAR *)&blocksizes[1], 4, bmi_handle);
    }
    /* Must initialize as for wifi1 it still carries prev values */
    board_id = 0;
    return EOK;

failed:
       qdf_info("%s : ol_transfer_bin_file failed. phase:%d ",__func__,phase);
       return status ;
}

#if !QCA_WIFI_QCA8074_VP
static QDF_STATUS
ol_get_tgt_dump_location(A_UINT32 *fw_io_mem_addr_l, A_UINT32 *fw_io_mem_size_l)
{
#if ATH_SUPPORT_FW_RAM_DUMP_FOR_MIPS
    /* MIPS PLATFORM */
    u_int32_t vmem_len = 0;

    if (ath79_get_wlan_fw_dump_buffer((void *)&fw_io_mem_addr_l, &vmem_len) == -1) {
        qdf_err("Cannot access memory to dump WLAN Firmware data");
        return QDF_STATUS_E_NOSUPPORT;
    }
    *fw_io_mem_size_l = vmem_len;
#else
    /* ARM PLATFORM */
#if  LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)
    struct device_node *dev_node=NULL;
    int num_addr_cell, num_size_cell, ret;
    unsigned int registerdetails[4]={0};
    /* get fw io mem addr & size entries from dtsi file */
    dev_node = of_find_node_by_name (NULL, "wifi_dump");
    if (dev_node) {
        num_addr_cell = of_n_addr_cells(dev_node);
        num_size_cell = of_n_size_cells(dev_node);
        if ((ret = of_property_read_u32_array(dev_node,"reg", &registerdetails[0], (2 * num_addr_cell)))) {
            qdf_err("Error: While retrieving register details from the reg entry in wifi_dump. error %d",ret);
            return QDF_STATUS_E_FAULT;
        }
        if ((num_addr_cell == 2) && (num_size_cell == 2)) {
            *fw_io_mem_addr_l = registerdetails[1];
            *fw_io_mem_size_l = registerdetails[3];
        } else if ((num_addr_cell == 1) && (num_size_cell == 1)) {
            *fw_io_mem_addr_l = registerdetails[0];
            *fw_io_mem_size_l = registerdetails[1];
        } else {
            qdf_err("Error in retrieving addr & size cells from the reg entry in wifi_dump");
            return QDF_STATUS_E_FAULT;
        }
    } else {
        qdf_err("ERROR: No wifi_dump dts node available in the dts entry file");
        return QDF_STATUS_E_NOENT;
    }
#else /* for linux version code < 3.14 */
    if (target_type == TARGET_TYPE_AR9888) {
        fw_io_mem_addr_l = FW_IO_MEM_ADDR_AR9888;
        fw_io_mem_size_l = FW_IO_MEM_SIZE_AR9888;
    } else if (target_type == TARGET_TYPE_QCA9984 || target_type == TARGET_TYPE_QCA9888) {
        fw_io_mem_addr_l = FW_IO_MEM_ADDR_AR900B;
        fw_io_mem_size_l = FW_IO_MEM_SIZE_AR900B;
    } else if (target_type == TARGET_TYPE_AR900B){
        fw_io_mem_addr_l = FW_IO_MEM_ADDR_AR900B;
        fw_io_mem_size_l = FW_IO_MEM_SIZE_AR900B;
    } else if (target_type == TARGET_TYPE_IPQ4019) {
        fw_io_mem_addr_l = FW_IO_MEM_ADDR_IPQ4019;
        fw_io_mem_size_l = FW_IO_MEM_SIZE_IPQ4019;
    } else {
        return QDF_STATUS_E_INVAL;
    }
#endif /* LINUX_VERSION_CODE */
#endif /* ARM/MIPS */

    qdf_info("Retrived fw_io_mem_size_l = %x fw_io_mem_addr_l = %x",*fw_io_mem_size_l, *fw_io_mem_addr_l);
    return QDF_STATUS_SUCCESS;
}
#endif /* QCA_WIFI_QCA8074_VP */

#if !QCA_WIFI_QCA8074_VP
/*
 * API to read the iram content from the target memory and write into the
 * F/W dump location.
 */
static void
ol_ath_copy_tgt_iram(struct ol_ath_soc_softc *soc)
{
    uint32_t tgt_iram_paddr;
    A_UINT32 *tgt_iram_bkp_vaddr;
    A_UINT32 fw_iram_bkp_addr, fw_io_mem_size_l;
    A_UINT32 *org_iram_vaddr;
    A_UINT32 offset; /* 2MB per radio */
    A_UINT8 num_iram_read_itr = 0;
    struct wlan_objmgr_psoc *psoc;
    struct target_psoc_info *tgt_hdl;
    struct hif_opaque_softc *hif_hdl;
    int ret, i = 0;

    /* Check if it is already backed up */
    if (soc->tgt_iram_bkp_paddr) {
        qdf_info("tgt iram backed up. host paddr 0x%x Radio id %d",
                 soc->tgt_iram_bkp_paddr, soc->soc_idx);
        return;
    }

    psoc = soc->psoc_obj;
    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(psoc);
    if (!tgt_hdl) {
        qdf_warn("psoc target_psoc_info is null");
        return;
    }
    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);

    tgt_iram_paddr = TARG_IRAM_START;
    ret = ol_get_tgt_dump_location(&fw_iram_bkp_addr, &fw_io_mem_size_l);

    /* Get crashdump mem addr and size */
#if ATH_SUPPORT_FW_RAM_DUMP_FOR_MIPS
    /* MIPS PLATFORM */

    if (ret == QDF_STATUS_E_NOSUPPORT) {
        qdf_err("Cannot access memory to copy tgt iram");
        return;
    }
    /* MIPS platform have just 2MB reserved for FW dump.
    Always use the first slot */
    offset = 0;
#else
    /* ARM PLATFORM */
    if (ret != QDF_STATUS_SUCCESS)
        return;
    offset = (soc->soc_idx * 0x200000); /* 2MB per radio */
#endif
    fw_iram_bkp_addr += offset;
    soc->tgt_iram_bkp_paddr = fw_iram_bkp_addr;
    tgt_iram_bkp_vaddr = ioremap_nocache(soc->tgt_iram_bkp_paddr,
                                         (TARG_IRAM_SZ + 1));
    if (!tgt_iram_bkp_vaddr) {
        qdf_warn("ioremap failed for tgt IRAM backup");
        soc->tgt_iram_bkp_paddr = 0;
        return;
    }
    org_iram_vaddr = tgt_iram_bkp_vaddr;
    qdf_info("tgt iram paddr: 0x%x host paddr 0x%x Radio id %d",
             tgt_iram_paddr, soc->tgt_iram_bkp_paddr, soc->soc_idx);

    num_iram_read_itr = TARG_IRAM_SZ / TGT_IRAM_READ_PER_ITR;
    qdf_debug("num_iram_read_itr: %u, TARG_IRAM_SZ: %u, TGT_IRAM_READ_PER_ITR: %u",
              num_iram_read_itr, TARG_IRAM_SZ, TGT_IRAM_READ_PER_ITR);

    for (i = 0; i < num_iram_read_itr; i++) {

        if (hif_diag_read_mem(hif_hdl, tgt_iram_paddr,
                              (A_UCHAR *)tgt_iram_bkp_vaddr,
                              TGT_IRAM_READ_PER_ITR)!= QDF_STATUS_SUCCESS) {
            qdf_warn("HifDiagReadMem FW IRAM content failed");
            soc->tgt_iram_bkp_paddr = 0;
        }
        tgt_iram_paddr += TGT_IRAM_READ_PER_ITR;
        tgt_iram_bkp_vaddr = (A_UINT32 *)((A_UCHAR *)tgt_iram_bkp_vaddr + TGT_IRAM_READ_PER_ITR);
    }
    iounmap(org_iram_vaddr);
    return;
}
#endif  /* QCA_WIFI_QCA8074_VP */

int ol_target_init(ol_ath_soc_softc_t *soc, bool first)
{
    int status = 0;
#ifdef AH_CAL_IN_FLASH_PCI
    u_int32_t cal_location;
#endif
    struct hif_target_info *tgt_info;
    uint32_t target_type;
    uint32_t target_version;
    struct wlan_objmgr_psoc *psoc;
    struct target_psoc_info *tgt_hdl;
    void *hif_hdl;

    psoc = soc->psoc_obj;
    tgt_hdl = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(psoc);
    if (!tgt_hdl) {
        qdf_info("%s: psoc target_psoc_info is null", __func__);
        return -EINVAL;
    }
    target_type = target_psoc_get_target_type(tgt_hdl);
    hif_hdl = target_psoc_get_hif_hdl(tgt_hdl);

#ifdef AH_CAL_IN_FLASH_PCI
#define HOST_CALDATA_SIZE (16 * 1024)
    soc->cal_in_flash = 1;
    cal_location = CalAddr[soc->cal_idx-1];
#ifndef ATH_CAL_NAND_FLASH
    soc->cal_mem = A_IOREMAP(cal_location, HOST_CALDATA_SIZE);
    if (!soc->cal_mem) {
        qdf_info("%s: A_IOREMAP failed", __func__);
        return -1;
    }
    else
    {
        qdf_info("\n soc->cal_mem %pK AH_CAL_LOCATIONS_PCI %x HOST_CALDATA_SIZE %x",
                soc->cal_mem, cal_location, HOST_CALDATA_SIZE);
    }
#endif
#endif
    soc->cal_in_file = 1;
    if (!hif_needs_bmi(hif_hdl)) {
        qdf_info("Skipping BMI");
    } else {
        hif_register_bmi_callbacks(hif_hdl);
    /*
     * 1. Initialize BMI
     */
    soc->bmi_handle = BMIAlloc();
    if(soc->bmi_handle == NULL) {
        qdf_info(" BMI alloc failed ");
        return -1;
    }
    BMIInit(soc->bmi_handle, soc->sc_osdev);
    qdf_info(KERN_INFO"%s() BMI inited.", __func__);

    if (!soc->is_sim) {
        unsigned int bmi_user_agent;
        struct bmi_target_info targ_info;

        /*
         * 2. Get target information
         */
        OS_MEMZERO(&targ_info, sizeof(targ_info));
        if (BMIGetTargetInfo(hif_hdl, &targ_info, soc->bmi_handle) != A_OK) {
            status = -1;
            goto attach_failed;
        }
        qdf_info(KERN_INFO"%s() BMI Get Target Info.", __func__);

        target_version = targ_info.target_ver;
	target_psoc_set_target_ver(tgt_hdl, target_version);

        tgt_info = hif_get_target_info_handle(hif_hdl);
        tgt_info->target_version = target_version;
        qdf_info("Chip id: 0x%x, chip version: 0x%x", target_type, target_version);
        ol_ath_host_config_update(soc);
        bmi_user_agent = ol_ath_bmi_user_agent_init(soc);
        if (bmi_user_agent) {
            /* User agent handles BMI phase */
            int rv;

            rv = ol_ath_wait_for_bmi_user_agent(soc);
            if (rv) {
                status = -1;
                goto attach_failed;
            }
        } else {
            /* Driver handles BMI phase */

            /*
             * 3. Configure target
             */
            if (ol_ath_configure_target(soc) != EOK) {
                status = -1;
                goto attach_failed;
            }
            qdf_info(KERN_INFO"%s() configure Target .", __func__);

            /*
             * 4. Download firmware image and data files
             */
            if ((status = ol_ath_download_firmware(soc)) != EOK)
            {
                goto attach_failed;
            }
            qdf_info(KERN_INFO"%s() Download FW done. ", __func__);
        }
    }
}
#if !QCA_WIFI_QCA8074_VP
    /* target iram backup for BL family only */
    if (target_type == TARGET_TYPE_QCA9984 ||
        target_type == TARGET_TYPE_IPQ4019 ||
        target_type == TARGET_TYPE_AR900B  ||
        target_type == TARGET_TYPE_QCA9888) {
        ol_ath_copy_tgt_iram(soc);
    }
#endif
    return 0;
attach_failed:
    return status;
}
qdf_export_symbol(ol_target_init);

static
int ol_derive_swap_filename (const char* bin_filename, char* swap_filename, ATH_SWAP_INFO file)
{

    char temp[MAX_SWAP_FILENAME], *split_name;
    const char* filename = bin_filename;
    int len =0, dot_count=0, tot_dotcount=0;
    split_name = &temp[0];

    if(!bin_filename) {
        qdf_info("%s: Bin file not present",__func__);
        return -1;
    }

    while (*filename) {
        if(*filename == '.')
            tot_dotcount++;
        filename++;
    }

    filename = bin_filename;

    while (*filename && len < MAX_SWAP_FILENAME) {
        if(*filename == '.')
            dot_count++;
        *split_name++ = *filename++;
        len++;
        if(dot_count == tot_dotcount)
            break;
    }

    if (len == MAX_SWAP_FILENAME)
        *(--split_name) = '\0';
    else
        *split_name = '\0';

    split_name = temp;

    switch (file) {

    case ATH_TARGET_OTP_CODE_SWAP:
           strlcat(split_name,TARGET_CODE_SWAP_FILE_EXT_TYPE,MAX_SWAP_FILENAME);
           break;

    case ATH_TARGET_OTP_DATA_SWAP:
           strlcat(split_name,TARGET_DATA_SWAP_FILE_EXT_TYPE,MAX_SWAP_FILENAME);
           break;

    case ATH_TARGET_BIN_CODE_SWAP:
           strlcat(split_name,TARGET_CODE_SWAP_FILE_EXT_TYPE,MAX_SWAP_FILENAME);
           break;

    case ATH_TARGET_BIN_DATA_SWAP:
           strlcat(split_name,TARGET_DATA_SWAP_FILE_EXT_TYPE,MAX_SWAP_FILENAME);
           break;

    case ATH_TARGET_BIN_UTF_CODE_SWAP:
           strlcat(split_name,TARGET_CODE_SWAP_FILE_EXT_TYPE,MAX_SWAP_FILENAME);
           break;

    case ATH_TARGET_BIN_UTF_DATA_SWAP:
           strlcat(split_name,TARGET_DATA_SWAP_FILE_EXT_TYPE,MAX_SWAP_FILENAME);
           break;

    default:
          break;

    }

   memcpy(swap_filename,split_name,strlen(split_name));
   return 1;
}


int
ol_transfer_swap_struct(ol_ath_soc_softc_t *soc, ATH_SWAP_INFO swap_info,char *bin_filename)
{

    struct hif_opaque_softc *sc;
    struct swap_seg_info *seg_info;
    const char *file_name = bin_filename;
    A_STATUS rv;
    u_int32_t target_write_addr;
    u_int64_t *cpu_addr;
    int status = -1;

    sc = lmac_get_ol_hif_hdl(soc->psoc_obj);

    if (!sc) {
        qdf_info("%s: hif_pci_softc is null", __func__);
        return status;
    }

    switch (swap_info) {

        case ATH_TARGET_OTP_CODE_SWAP:
            /*check for previous swap seg alloc*/
            if(!soc->target_otp_codeswap_seginfo) {
                /* Allocate swap seg  */
                status = ol_swap_seg_alloc (soc, &seg_info, &cpu_addr, file_name, swap_info);
                /*successfull swap seg alloc*/
                if (status == 0) {
                    soc->target_otp_codeswap_seginfo = seg_info;
                    soc->target_otp_codeswap_cpuaddr = cpu_addr;
                }
            } else {
                /* reuse the previously allocated swap seg info */
                seg_info = soc->target_otp_codeswap_seginfo;
                status = 0;
            }
            break;

        case ATH_TARGET_OTP_DATA_SWAP:
            /*check for previous swap seg alloc*/
            if(!soc->target_otp_dataswap_seginfo) {
                /* Allocate swap seg  */
                status = ol_swap_seg_alloc (soc, &seg_info, &cpu_addr, file_name, swap_info);
                /*successfull swap seg alloc*/
                if (status == 0) {
                    soc->target_otp_dataswap_seginfo = seg_info;
                    soc->target_otp_dataswap_cpuaddr = cpu_addr;
                }
            } else {
                /* reuse the previously allocated swap seg info */
                seg_info = soc->target_otp_dataswap_seginfo;
                status = 0;
            }
            break;

        case ATH_TARGET_BIN_CODE_SWAP:
            /*check for previous swap seg alloc*/
            if(!soc->target_bin_codeswap_seginfo) {
                /* Allocate swap seg  */
                status = ol_swap_seg_alloc (soc, &seg_info, &cpu_addr, file_name, swap_info);
                /*successfull swap seg alloc*/
                if (status == 0) {
                    soc->target_bin_codeswap_seginfo = seg_info;
                    soc->target_bin_codeswap_cpuaddr = cpu_addr;
                }
            } else {
                /* reuse the previously allocated swap seg info */
                seg_info = soc->target_bin_codeswap_seginfo;
                status = 0;
            }
            break;

        case ATH_TARGET_BIN_DATA_SWAP:
            /*check for previous swap seg alloc*/
            if(!soc->target_bin_dataswap_seginfo) {
                /* Allocate swap seg  */
                status = ol_swap_seg_alloc (soc, &seg_info, &cpu_addr, file_name, swap_info);
                /*successfull swap seg alloc*/
                if (status == 0) {
                    soc->target_bin_dataswap_seginfo = seg_info;
                    soc->target_bin_dataswap_cpuaddr = cpu_addr;
                }
            } else {
                /* reuse the previously allocated swap seg info */
                seg_info = soc->target_bin_dataswap_seginfo;
                status = 0;
            }
            break;

        case ATH_TARGET_BIN_UTF_CODE_SWAP:
            /*check for previous swap seg alloc*/
            if(!soc->target_bin_utf_codeswap_seginfo) {
                /* Allocate swap seg  */
                status = ol_swap_seg_alloc (soc, &seg_info, &cpu_addr, file_name, swap_info);
                /*successfull swap seg alloc*/
                if (status == 0) {
                    soc->target_bin_utf_codeswap_seginfo = seg_info;
                    soc->target_bin_utf_codeswap_cpuaddr = cpu_addr;
                }
            } else {
                /* reuse the previously allocated swap seg info */
                seg_info = soc->target_bin_utf_codeswap_seginfo;
                status = 0;
            }
            break;

        case ATH_TARGET_BIN_UTF_DATA_SWAP:
            /*check for previous swap seg alloc*/
            if(!soc->target_bin_utf_dataswap_seginfo) {
                /* Allocate swap seg  */
                status = ol_swap_seg_alloc (soc, &seg_info, &cpu_addr, file_name, swap_info);
                /*successfull swap seg alloc*/
                if (status == 0) {
                    soc->target_bin_utf_dataswap_seginfo = seg_info;
                    soc->target_bin_utf_dataswap_cpuaddr = cpu_addr;
                }
            } else {
                /* reuse the previously allocated swap seg info */
                seg_info = soc->target_bin_utf_dataswap_seginfo;
                status = 0;
            }
            break;

        default :
            break;

    }

    if (status !=0 ) {
        qdf_info("%s: Swap Seg alloc failed for FW bin type %d ",__func__,swap_info);
        return status;
    }

    /* Parse the code/data swap file and copy to the host memory & get target write addr */
    if (ol_swap_wlan_memory_expansion(soc,seg_info,file_name,&target_write_addr)) {
        qdf_info("%s: Swap Memory expansion failed for FW bin type %d ",__func__,swap_info);
        status = -1;
        return status;
    }

    /* Write the code/data seg info to the target addr  obtained above memory expansion*/
    rv  = BMIWriteMemory(sc, target_write_addr,
            (u_int8_t *)seg_info, sizeof(struct swap_seg_info), soc->bmi_handle);

    qdf_info(KERN_INFO"soc=%pK  target_write_addr=%x seg_info=%pK ", soc,target_write_addr,(u_int8_t *)seg_info);

    if (rv != A_OK) {
        qdf_info("Failed to Write for Target Memory Expansion target_addr=%x ",target_write_addr);
        return rv;
    }
    qdf_info(KERN_INFO"%s:Code swap structure successfully downloaded for bin type =%d ",__func__,swap_info);
    return 0;
}

int
ol_ath_code_data_swap(ol_ath_soc_softc_t *soc,const char * bin_filename, ATH_BIN_FILE file_type)
{

    char swap_filename[FILE_PATH_LEN];
    char filepath [FILE_PATH_LEN];
    const char *filename_path = filepath;
    struct file *file;
    int status =0;
    int len = 0;

    if(!bin_filename)
        return status;

    switch (file_type) {

        case ATH_OTP_FILE:
    	     memset(&swap_filename[0], 0, sizeof(swap_filename));
             ol_derive_swap_filename(bin_filename, swap_filename, ATH_TARGET_OTP_CODE_SWAP);

             memset(&filepath[0], 0, sizeof(filepath));
             strlcpy(filepath,SWAP_FILE_PATH,FILE_PATH_LEN);
             len = strlen(filepath);
             len += strlen(swap_filename);
             if(len <= (FILE_PATH_LEN-1)) {
                strlcat(filepath,swap_filename,FILE_PATH_LEN);
                len = 0;
             }
             else {
                 qdf_info("%s:%d file path len exceeds array len",__func__,__LINE__);
                 return -ENAMETOOLONG;
             }

            /* Target OTP  Code Swap & Data swap should always happen
             * before downloading the actual OTP binary  file  */
             file = file_open(filename_path, O_RDONLY, 00644);
             if (file) {
                 file_close(file);
                 /* Allocate, expand & write otp code swap address to fw */
                 status = ol_transfer_swap_struct(soc,ATH_TARGET_OTP_CODE_SWAP,swap_filename);
                 if(status != 0) {
                     return status;
                 }
                 qdf_info("bin_filename=%s swap_filename=%s ",bin_filename, filepath);
             }

    	     memset(&swap_filename[0], 0, sizeof(swap_filename));
             ol_derive_swap_filename(bin_filename, swap_filename, ATH_TARGET_OTP_DATA_SWAP);

             memset(&filepath[0], 0, sizeof(filepath));
             strlcpy(filepath,SWAP_FILE_PATH,FILE_PATH_LEN);
             len = strlen(filepath);
             len += strlen(swap_filename);
             if(len <= (FILE_PATH_LEN-1)) {
                strlcat(filepath,swap_filename,FILE_PATH_LEN);
                len = 0;
             }
             else {
                 qdf_info("%s:%d file path len exceeds array len",__func__,__LINE__);
                 return -ENAMETOOLONG;
             }

             file = file_open(filename_path, O_RDONLY, 00644);
             if (file) {
                 file_close(file);

                 /* Allocate, expand & write otp code swap address to fw */
                 status = ol_transfer_swap_struct(soc,ATH_TARGET_OTP_DATA_SWAP,swap_filename);
                 if(status != 0) {
                     return status;
                 }
                 qdf_info("bin_filename=%s swap_filename=%s ",bin_filename, filepath);
             }
             break;

        case ATH_FIRMWARE_FILE:
    	     memset(&swap_filename[0], 0, sizeof(swap_filename));
             ol_derive_swap_filename(bin_filename, swap_filename, ATH_TARGET_BIN_CODE_SWAP);

	     memset(&filepath[0], 0, sizeof(filepath));
             strlcpy(filepath,SWAP_FILE_PATH,FILE_PATH_LEN);
             len = strlen(filepath);
             len += strlen(swap_filename);
             if(len <= (FILE_PATH_LEN-1)) {
                strlcat(filepath,swap_filename,FILE_PATH_LEN);
                len = 0;
             }
             else {
                 qdf_info("%s:%d file path len exceeds array len",__func__,__LINE__);
                 return -ENAMETOOLONG;
             }

             /* Target fw bin Code Swap & Data swap should always
              * happen before downloading the actual fw binary  file  */
	     file = file_open(filename_path, O_RDONLY, 00644);
             if (file) {
                 file_close(file);
                 /* Allocate, expand & write otp code swap address to fw */
                 status = ol_transfer_swap_struct(soc,ATH_TARGET_BIN_CODE_SWAP,swap_filename);
                 if(status != 0) {
                    return status;
                 }
                 qdf_info(KERN_INFO"bin_filename=%s swap_filename=%s ",bin_filename, filepath);
             }

    	     memset(&swap_filename[0], 0, sizeof(swap_filename));
             ol_derive_swap_filename(bin_filename, swap_filename, ATH_TARGET_BIN_DATA_SWAP);

	     memset(&filepath[0], 0, sizeof(filepath));
             strlcpy(filepath,SWAP_FILE_PATH,FILE_PATH_LEN);
             len = strlen(filepath);
             len += strlen(swap_filename);
             if(len <= (FILE_PATH_LEN-1)) {
                strlcat(filepath,swap_filename,FILE_PATH_LEN);
                len = 0;
             }
             else {
                 qdf_info("%s:%d file path len exceeds array len",__func__,__LINE__);
                 return -ENAMETOOLONG;
             }

             file = file_open(filename_path, O_RDONLY, 00644);
             if (file) {
                 file_close(file);
                 /* Allocate, expand & write otp code swap address to fw */
                 status = ol_transfer_swap_struct(soc,ATH_TARGET_BIN_DATA_SWAP,swap_filename);
                 if(status != 0) {
                    return status;
                 }
                 qdf_info("bin_filename=%s swap_filename=%s ",bin_filename, filepath);
             }
             break;

        case ATH_UTF_FIRMWARE_FILE:
             memset(&swap_filename[0], 0, sizeof(swap_filename));
             ol_derive_swap_filename(bin_filename, swap_filename, ATH_TARGET_BIN_UTF_CODE_SWAP);

             memset(&filepath[0], 0, sizeof(filepath));
             strlcpy(filepath,SWAP_FILE_PATH,FILE_PATH_LEN);
             len = strlen(filepath);
             len += strlen(swap_filename);
             if(len <= (FILE_PATH_LEN-1)) {
                strlcat(filepath,swap_filename,FILE_PATH_LEN);
                len = 0;
             }
             else {
                 qdf_info("%s:%d file path len exceeds array len",__func__,__LINE__);
                 return -ENAMETOOLONG;
             }

             /* Target fw bin Code Swap & Data swap should always
              * happen before downloading the actual fw binary  file  */
             file = file_open(filename_path, O_RDONLY, 00644);
             if (file) {
                 file_close(file);
                 /* Allocate, expand & write otp code swap address to fw */
                 status = ol_transfer_swap_struct(soc,ATH_TARGET_BIN_UTF_CODE_SWAP,swap_filename);
                 if(status != 0) {
                    return status;
                 }
                 qdf_info("bin_filename=%s swap_filename=%s ",bin_filename, filepath);
             }

             memset(&swap_filename[0], 0, sizeof(swap_filename));
             ol_derive_swap_filename(bin_filename, swap_filename, ATH_TARGET_BIN_UTF_DATA_SWAP);

             memset(&filepath[0], 0, sizeof(filepath));
             strlcpy(filepath,SWAP_FILE_PATH, sizeof(filepath));
             len = strlen(filepath);
             len += strlen(swap_filename);
             if(len <= (FILE_PATH_LEN-1)) {
                strlcat(filepath,swap_filename,FILE_PATH_LEN);
                len = 0;
             }
             else {
                 qdf_info("%s:%d file path len exceeds array len",__func__,__LINE__);
                 return -ENAMETOOLONG;
             }

             file = file_open(filename_path, O_RDONLY, 00644);
             if (file) {
                 file_close(file);
                 /* Allocate, expand & write otp code swap address to fw */
                 status = ol_transfer_swap_struct(soc,ATH_TARGET_BIN_UTF_DATA_SWAP,swap_filename);
                 if(status != 0) {
                    return status;
                 }
                 qdf_info("bin_filename=%s swap_filename=%s ",bin_filename, filepath);
             }
             break;

        default:
             break;

    }
    return status;
}



#define SOC_PCIE_REGBASE 0x81030
#define PCIE_BAR0_MASK 0xfffff
int ol_diag_read_sram(struct hif_opaque_softc *hif_ctx, unsigned int sram_base,
		void *dst, size_t len)
{
	int i=0;
	unsigned int target_bar0;

	if (!dst)
		return -ENOMEM;
	/* get the bar0 from target register */

	target_bar0 = hif_reg_read(hif_ctx, SOC_PCIE_REGBASE);

	/* clear off the bits 0-19 */
	target_bar0 = target_bar0 & ~PCIE_BAR0_MASK;

	for (i=0; i < len; i+=4) {
		hif_reg_write(hif_ctx, 0x4d00c, target_bar0+sram_base+i);
		*(unsigned int*)dst = hif_reg_read(hif_ctx, 0x4d010);
		dst += 4;
	}
	return EOK;
}

/*
 * Debugging Target Crash Log only: Do not modify anything
 */
struct fw_ram_dump {
        /* Take highest Length here Currently DRAM */
        A_UINT8 dram[FW_DRAM_LENGTH_QCA9984];
        A_UINT8 sram[FW_SRAM_LENGTH];
        A_UINT8 iram[FW_IRAM_LENGTH];
        A_UINT8 apb_reg_space[HW_APB_REG_LENGTH];
        A_UINT8 wifi_reg_space[HW_WIFI_REG_LENGTH];
        A_UINT8 ce_reg_space[HW_CE_REG_LENGTH];
        A_UINT8 soc_reg_space[SOC_REG_LENGTH];
        A_UINT8 apb_reg_space1[HW_APB_REG_LENGTH1];
};
enum {
    FW_DRAM_IDX=0,
    FW_SRAM_IDX,
    FW_IRAM_IDX,
    FW_APB0_REG_IDX,
    FW_WIFI_REG_IDX,
    FW_CE_REG_IDX,
    FW_SOC_REG_IDX,
    FW_APB1_REG_IDX,
    FW_REG_MAX
};
struct fw_ram_dump_map {
    A_UINT32 address;
    A_UINT32 length;
    A_UINT8  skip;
    QDF_STATUS (*CustomHIFDiagReadMem)(struct hif_opaque_softc *hif_device, uint32_t sourceaddr, uint8_t *destbuf, int nbytes);

} fw_ram_dump_map_AR9888 [FW_REG_MAX] = {
    /* address in fw    length              skip */
    {FW_DRAM_ADDRESS,   FW_DRAM_LENGTH_AR9888,0, NULL},
    {FW_SRAM_ADDRESS,   FW_SRAM_LENGTH,     0,  NULL},
    {FW_IRAM_ADDRESS,   FW_IRAM_LENGTH,     0,  NULL},
    {HW_APB_REG_SPACE,  HW_APB_REG_LENGTH,  0,  NULL},
    {HW_WIFI_REG_SPACE, HW_WIFI_REG_LENGTH, 0,  NULL},
    {HW_CE_REG_SPACE,   HW_CE_REG_LENGTH,   0,  NULL},
    {SOC_REG_SPACE,     SOC_REG_LENGTH,     0,  NULL},
    {HW_APB_REG_SPACE1, HW_APB_REG_LENGTH1, 0,  NULL}
},
fw_ram_dump_map_AR900B [FW_REG_MAX] = {
    /* address in fw    length              skip */
    {FW_DRAM_ADDRESS,   FW_DRAM_LENGTH_AR900B,0, NULL},
    {FW_SRAM_ADDRESS,   FW_SRAM_LENGTH,     0,  NULL},
    {FW_IRAM_ADDRESS,   FW_IRAM_LENGTH,     0,  NULL},
    {HW_APB_REG_SPACE,  HW_APB_REG_LENGTH,  0,  NULL},
    {HW_WIFI_REG_SPACE, HW_WIFI_REG_LENGTH, 0,  NULL},
    {HW_CE_REG_SPACE,   HW_CE_REG_LENGTH,   0,  NULL},
    {SOC_REG_SPACE,     SOC_REG_LENGTH,     0,  NULL},
    {HW_APB_REG_SPACE1, HW_APB_REG_LENGTH1, 0,  NULL}
},
fw_ram_dump_map_QCA9984 [FW_REG_MAX] = {
    /* address in fw    length              skip */
    {FW_DRAM_ADDRESS,   FW_DRAM_LENGTH_QCA9984,0, NULL},
    {FW_SRAM_ADDRESS,   FW_SRAM_LENGTH,     0,  NULL},
    {FW_IRAM_ADDRESS,   FW_IRAM_LENGTH,     0,  NULL},
    {HW_APB_REG_SPACE,  HW_APB_REG_LENGTH,  0,  NULL},
    {HW_WIFI_REG_SPACE, HW_WIFI_REG_LENGTH, 0,  NULL},
    {HW_CE_REG_SPACE,   HW_CE_REG_LENGTH,   0,  NULL},
    {SOC_REG_SPACE,     SOC_REG_LENGTH,     0,  NULL},
    {HW_APB_REG_SPACE1, HW_APB_REG_LENGTH1, 0,  NULL}
}
#ifdef ATH_AHB
,
fw_ram_dump_map_IPQ4019 [FW_REG_MAX] = {
    /* address in fw    length              skip */
    {FW_DRAM_ADDRESS,   FW_DRAM_LENGTH_IPQ4019,0, NULL},
    {FW_SRAM_ADDRESS,   FW_SRAM_LENGTH,     0,  NULL},
    {FW_IRAM_ADDRESS,   FW_IRAM_LENGTH,     0,  NULL},
    {HW_APB_REG_SPACE,  HW_APB_REG_LENGTH,  0,  NULL},
    {HW_WIFI_REG_SPACE, HW_WIFI_REG_LENGTH, 0,  NULL},
    {HW_CE_REG_SPACE,   HW_CE_REG_LENGTH,   0,  NULL},
#if OL_ATH_SUPPORT_LED
    {SOC_REG_SPACE,   SOC_REG_LENGTH,     0,    hif_diag_read_soc_ipq4019},
#else
    {SOC_REG_SPACE,   SOC_REG_LENGTH,     0,    NULL},
#endif
    {HW_APB_REG_SPACE1, HW_APB_REG_LENGTH1, 0,  NULL}
}
#endif
;


char radioasserted[2] = {0,0};
/*
 * This function creates the core dump either into file or pre-allocated
 * memory or into both.
 */
#if !QCA_WIFI_QCA8074_VP
static void
fw_get_core_dump(ol_ath_soc_softc_t *ramdump_soc, A_INT8 *file_path,
        A_UINT32 d_opts)
{
    struct file *file = NULL;
    A_UINT32 length, i=0;
    A_UINT32 address;
    void *crash_scope_addr = NULL;      /* for dumping to crash scope */
    void *scratch_buf =  NULL;          /* srach buffer for copying*/
    void *scratch_buf_start = NULL;
    A_UINT32 dump_to_file  = d_opts & FW_DUMP_TO_FILE;
    A_UINT32 dump_to_scope =  d_opts & FW_DUMP_TO_CRASH_SCOPE;
    A_UINT32 n_file_wr_offset = 0;
    A_UINT32 n_buf_wr_offset = 0;
    int dump_to_scope_ovf = 0;
    struct fw_ram_dump_map *fw_ram_dump_map_cmn = NULL;
    A_UINT32 fw_io_mem_addr_l = 0;
    A_UINT32 fw_io_mem_size_l = 0;
    int fw_dump_max_idx = 0;
    A_STATUS status=0;
    QDF_STATUS retv;

#define MAX_ADAPTIVE_BUFF_LEN   1024
#define MIN_ADAPTIVE_BUFF_LEN   128
    int print_scope = 0;
    int block_size = 0;
    int remaining = 0, coreid = 0;
    int adaptive_buff_len = MAX_ADAPTIVE_BUFF_LEN;
    struct hif_opaque_softc *hif_hdl = NULL;
    uint32_t target_type;
    int full_mem_alloced = 0;
    int total_mem_length = 0;

    if (!ramdump_soc) {
        qdf_info("ramdump soc is NULL");
        qdf_target_assert_always(0);
        return;
    }

    target_type = lmac_get_tgt_type(ramdump_soc->psoc_obj);

    hif_hdl = lmac_get_ol_hif_hdl(ramdump_soc->psoc_obj);

    if (!hif_hdl) {
        qdf_info("hif handle is NULL: %s ", __func__);
        return;
    }

    if (target_type == TARGET_TYPE_AR9888) {
       fw_ram_dump_map_cmn = (struct fw_ram_dump_map *)&fw_ram_dump_map_AR9888;
    } else if (target_type == TARGET_TYPE_QCA9984 || target_type == TARGET_TYPE_QCA9888) {
       fw_ram_dump_map_cmn = (struct fw_ram_dump_map *)&fw_ram_dump_map_QCA9984;
    }
#ifdef ATH_AHB
    else if (target_type == TARGET_TYPE_IPQ4019) {
       fw_ram_dump_map_cmn = (struct fw_ram_dump_map *)&fw_ram_dump_map_IPQ4019;
    }
#endif
    else {
       fw_ram_dump_map_cmn = (struct fw_ram_dump_map *)&fw_ram_dump_map_AR900B;
    }

    retv = ol_get_tgt_dump_location(&fw_io_mem_addr_l, &fw_io_mem_size_l);
    /* Get crashdump mem addr and size */
#if ATH_SUPPORT_FW_RAM_DUMP_FOR_MIPS
    /* MIPS PLATFORM */

    (void) coreid; // unused parameter
    if (dump_to_scope) {
        if (retv == QDF_STATUS_E_NOSUPPORT) {
            qdf_info("Cannot access memory to dump WLAN Firmware data");
            goto error_1;
        }
        crash_scope_addr = (void *)fw_io_mem_addr_l;

        /* MIPS platform have just 2MB reserved for FW dump.
        Always use the first slot */
        n_buf_wr_offset = 0;
    }
#else
    /* ARM PLATFORM */
    if (retv == QDF_STATUS_E_FAULT) {
        qdf_target_assert_always(0);
        return;
    } else if (retv == QDF_STATUS_E_NOENT) {
        qdf_target_assert_always(0);
        return;
    } else if (retv == QDF_STATUS_E_INVAL) {
        qdf_info("ERROR: unknown target type");
        return;
    }
    qdf_info("Retrived fw_io_mem_size_l = %x fw_io_mem_addr_l = %x",fw_io_mem_size_l,fw_io_mem_addr_l);

    if (fw_io_mem_size_l < (6 << 20)) {
        /* cannot assign slots as fw io memsize < 6MB, use slot0 only */
        coreid = 0;
        radioasserted[0] = 1;
        n_buf_wr_offset = 0;
    } else {
        if (ramdump_soc->soc_idx == 0) {
            coreid = 0;
            radioasserted[0] = 1;
        } else if (ramdump_soc->soc_idx == 1) {
            coreid = 1;
            radioasserted[1] = 1;
        } else if (ramdump_soc->soc_idx == 2) {
            if(radioasserted[0] != 1)
                coreid = 0;
            else if(radioasserted[1] != 1)
                coreid = 1;
            else {
                qdf_info("ERROR: No space left to copy the target contents for this radio wifi2.Using 3rd Slot");
                coreid = 2;
            }
            qdf_info("WARNING: No fixed crashdummp space defined yet to handle target assert for this radio wifi2");
            qdf_info("For now, copying the target assert in  slot [%d]",coreid);
        } else {
            qdf_info("ERROR: Invalid radio id");
            qdf_target_assert_always(0);
            return;
        }
        n_buf_wr_offset += (coreid * 0x200000); //choose the appropriate 2MB slot

        if((n_buf_wr_offset + 0x200000) > fw_io_mem_size_l) {
            qdf_info("ERROR: Trying write beyond offset %x is invalid.  ",fw_io_mem_size_l);
            qdf_target_assert_always(0);
            return;
        }
    }
    crash_scope_addr = (struct fw_ram_dump *)ioremap(fw_io_mem_addr_l, fw_io_mem_size_l);

#endif /* MIPS/ARM */

    /* check if dump_to_scope is required, if needed map the memory */
    if (dump_to_scope) {
        if (!crash_scope_addr) {
            dump_to_scope =  0;
            qdf_info("** %s: ioremap failure addr:0x%x, size:0x%x",
                   __func__, fw_io_mem_addr_l, fw_io_mem_size_l);
        } else {
            qdf_mem_set_io(crash_scope_addr+n_buf_wr_offset, 0x200000, 0); //reset the contents in the slot
        }
    }

    qdf_info("Copying %s target assert at offset 0x%x",ramdump_soc->sc_osdev->netdev->name,n_buf_wr_offset);

    /* DRAM is the biggest memory chunk, get the memory of that size,
     * and reuse this buffer for every region
     */

    if (lmac_is_target_ar900b(ramdump_soc->psoc_obj)) {
        fw_dump_max_idx = FW_REG_MAX;
    } else {
        /* Dump only dram as it supports only dram */
        fw_dump_max_idx = FW_SRAM_IDX;
    }


    /* Calculate total length */
    for (i = FW_DRAM_IDX; i < fw_dump_max_idx; i++)
    {
        total_mem_length += fw_ram_dump_map_cmn[i].length;
    }
    /* We will try to allocate MAX_ADAPTIVE_BUFF_LEN size off
     * scratch buffer for core dump generation. If it fails
     * beff length will be reduced to half and retried. This
     * scheme will try to allocat a maximum size buffer between
     * MAX_ADAPTIVE_BUFF_LEN and MIN_ADAPTIVE_BUFF_LEN.
     */
    for (adaptive_buff_len = total_mem_length; adaptive_buff_len >= MIN_ADAPTIVE_BUFF_LEN;
         adaptive_buff_len >>= 1) {
        scratch_buf_start = scratch_buf = OS_MALLOC(ramdump_soc->sc_osdev, adaptive_buff_len, GFP_KERNEL);
        if (scratch_buf) {
            if (adaptive_buff_len == total_mem_length) {
               full_mem_alloced = 1;
            }
            break;
        }
    }
    if (!scratch_buf) {
        qdf_info("** Scratch buff allocation failure, Core dump will not be created");
        goto error_1;
    }

    if (dump_to_scope && crash_scope_addr) {
        print_scope = 1;
    }

    for (i = FW_DRAM_IDX; i < fw_dump_max_idx; i++)
    {
        address = fw_ram_dump_map_cmn[i].address;
        length = fw_ram_dump_map_cmn[i].length;
        remaining = length;
        block_size = adaptive_buff_len;
        qdf_info("Copying target mem region[%d] 0x%X to crashscope location",i,address);
        if (!fw_ram_dump_map_cmn[i].skip) {
            while (remaining > 0){
                if (remaining < block_size) {
                    block_size = remaining;
                }
                qdf_mem_set_io(scratch_buf, block_size, 0);
                /* sram needs different handling */

		if (fw_ram_dump_map_cmn[i].CustomHIFDiagReadMem) {
                    status = (fw_ram_dump_map_cmn[i].CustomHIFDiagReadMem)(hif_hdl, address, (A_UCHAR*)scratch_buf, block_size);
                    if(status != A_OK) {
                        qdf_info("CustomHIFDiagReadMem failed for region %d will contain zeros", i);
                    }
                }
                else {
                    if (FW_SRAM_IDX == i && target_type != TARGET_TYPE_IPQ4019) {
                        /* for SRAM, we do not need to worry about virt address,
                         * because that is going to be read as 32 bit words;
                         */
                        ol_diag_read_sram(hif_hdl, address, (A_UCHAR*)scratch_buf, block_size);
                    } else {
                        if (hif_diag_read_mem(hif_hdl, address,
                                    (A_UCHAR*)scratch_buf,
                                    block_size)!= QDF_STATUS_SUCCESS) {
                            qdf_info("hif_diag_read_mem failed for region %d will contain zeros", i);
                        }
                    }
                }
                if (print_scope && !dump_to_scope_ovf) {
                    if ((n_buf_wr_offset + block_size ) < fw_io_mem_size_l) {
                        qdf_mem_copy_toio((A_UCHAR * )crash_scope_addr + n_buf_wr_offset, (A_UCHAR * )scratch_buf, block_size);
                        n_buf_wr_offset += block_size;
                    } else {
                        qdf_info("*** dump is overflowing probable loss of data***");
                        dump_to_scope_ovf = 1;
                    }
                }

                if (dump_to_file) {
                    if (!file_path){
                        dump_to_file = 0;
                    } else if (full_mem_alloced != 1) {
                        /* dump this segment to file */
                        qdf_fs_write(file_path, n_file_wr_offset, block_size, ((unsigned char*)scratch_buf));
                        n_file_wr_offset += block_size;
                        msleep(100);
                        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, ".");
                    }
                }
                if (full_mem_alloced == 1) {
                    scratch_buf += block_size;
                }
                address += block_size;
                remaining -= block_size;
            }
        }
    }
    if (dump_to_file && full_mem_alloced == 1) {
        /* dump this segment to file */
        qdf_fs_write(file_path, n_file_wr_offset, total_mem_length, ((unsigned char*)scratch_buf_start));
    }
    QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "%s *** dump collection complete %pK %pK \n", __func__, file_path, file);

error_1:
    if (dump_to_file && file){
        file_close(file);
    }
    if(dump_to_scope && crash_scope_addr) {
#if !ATH_SUPPORT_FW_RAM_DUMP_FOR_MIPS
        iounmap(crash_scope_addr);
#endif
    }
    if (full_mem_alloced != 1) {
        if (scratch_buf) {
            OS_FREE(scratch_buf);
            scratch_buf = NULL;
        }
    } else {
        if (scratch_buf_start) {
            OS_FREE(scratch_buf_start);
            scratch_buf_start = NULL;
        }
    }

}
#else
static void
fw_get_core_dump(ol_ath_soc_softc_t *ramdump_soc, A_INT8 *file_path,
        A_UINT32 d_opts)
{
	return;
}
#endif

int get_next_dump_file_index(ol_ath_soc_softc_t *soc, char *fw_dump_file)
{
    int next_file = 0;
    uint32_t target_type;

    target_type =  lmac_get_tgt_type(soc->psoc_obj);

    get_fileindex((char*)&next_file);

    /* get new file name with number ext */
    OS_MEMZERO(fw_dump_file, sizeof(fw_dump_file));

    if (target_type == TARGET_TYPE_AR900B) {
        snprintf(fw_dump_file, sizeof(FW_DUMP_FILE_AR900B)+sizeof(next_file)+1,
		"%s.%d",FW_DUMP_FILE_AR900B, next_file);
    } else if (target_type == TARGET_TYPE_QCA9984) {
        snprintf(fw_dump_file, sizeof(FW_DUMP_FILE_QCA9984)+sizeof(next_file)+1,
		"%s.%d",FW_DUMP_FILE_QCA9984, next_file);
    } else if (target_type == TARGET_TYPE_QCA9888) {
        snprintf(fw_dump_file, sizeof(FW_DUMP_FILE_QCA9888)+sizeof(next_file)+1,
		"%s.%d",FW_DUMP_FILE_QCA9888, next_file);
    } else if(target_type == TARGET_TYPE_IPQ4019) {
        snprintf(fw_dump_file, sizeof(FW_DUMP_FILE_IPQ4019)+sizeof(next_file)+1,
		"%s.%d",FW_DUMP_FILE_IPQ4019, next_file);
    } else {
        snprintf(fw_dump_file, sizeof(FW_DUMP_FILE_AR9888)+sizeof(next_file)+1,
		"%s.%d",FW_DUMP_FILE_AR9888, next_file);
    }
    /* next time new number */
    update_fileindex(next_file+1);
    return next_file;
}

void
ol_ath_dump_target(ol_ath_soc_softc_t *soc)
{
    char fw_dump_file[128]={0};

    get_next_dump_file_index(soc, fw_dump_file);
    qdf_info("** STARTING RUNTIME DUMP TARGET TO %s", fw_dump_file);
    fw_get_core_dump(soc, fw_dump_file, 1);
    qdf_info("*** RUNTIME DUMP TARGET COMPLETE ***");
}
qdf_export_symbol(ol_ath_dump_target);

atomic_t target_assert_count = ATOMIC_INIT(0);
void ramdump_work_handler(void *soc)
{
    ol_ath_soc_softc_t *ramdump_soc = soc;
#if !BUILD_X86
    char fw_dump_file[128]={0};
#endif
    A_UINT32 d_opts ;
#if UMAC_SUPPORT_ACFG
    struct pdev_op_args oper;
#endif

    if (!ramdump_soc) {
        qdf_info("ramdump soc is NULL");
        return;
    }
    d_opts = ramdump_soc->sc_dump_opts;
    /* no dump required, just return*/
    if ((d_opts & FW_DUMP_RECOVER_WITHOUT_CORE) &&
            (ramdump_soc->recovery_enable) ) {
        return;
    }

    /* recovery is enabled, but dump is not requested*/
    if (ramdump_soc->recovery_enable &&
            (d_opts & FW_DUMP_RECOVER_WITHOUT_CORE)) {
        return;
    }

	/*
	 * Do not worry now about CrashScope. Dump into file
     */
#if BUILD_X86
    qdf_info( "*** DO NOT CRASH X86, YOU SHOULD UNLOAD DRIVER HERE AFTER *** ");
    /* for X86 station version, there is no crash scope, and cannot crash */
    d_opts |= FW_DUMP_NO_HOST_CRASH;
    d_opts  &= ~(FW_DUMP_TO_CRASH_SCOPE);
#else
    /* get the next dump file */
    get_next_dump_file_index(soc, fw_dump_file);
    qdf_info("** STARTING DUMP options:%x", d_opts);
    fw_get_core_dump(ramdump_soc, fw_dump_file, d_opts);
    qdf_info("*** TARGET ASSERT DUMP COLLECTION COMPLETE ***");
#endif
#if UMAC_SUPPORT_ACFG
    oper.type = PDEV_ITER_TARGET_FWDUMP;
    wlan_objmgr_iterate_obj_list(ramdump_soc->psoc_obj, WLAN_PDEV_OP,
              wlan_pdev_operation, &oper, 0, WLAN_MLME_NB_ID);
#endif
    if (!ramdump_soc->recovery_enable && !(d_opts & FW_DUMP_NO_HOST_CRASH)) {
	    if(atomic_dec_and_test(&target_assert_count)) {
		    qdf_target_assert_always(0);
		    /*  this is return of no where, we should ideally wait here */
	    } else {
		    qdf_info("Do not reboot the board now. There is another target assert in the other radio");
	    }
    }
    return;
}
qdf_export_symbol(ramdump_work_handler);

#ifdef CONFIG_AR900B_SUPPORT
int
ol_get_board_id(ol_ath_soc_softc_t *soc, char *boarddata_file )
{
    char *filebuf = NULL;
    int buflen = 0;
    uint32_t target_type;

    filebuf = qdf_mem_malloc(MAX_FILENAMES_SIZE);
    target_type =  lmac_get_tgt_type(soc->psoc_obj);

    if(!filebuf) {
        qdf_info("\n %s : Alloc failed",__FUNCTION__);
        return -1;
    }
    /* For besra,there is NO number of boardfiles like cascade.*/
    if ((target_type == TARGET_TYPE_QCA9984 && cfg_get(soc->psoc_obj, CFG_OL_EMU_TYPE) != 0)
                || (target_type == TARGET_TYPE_IPQ4019) || \
                (target_type == TARGET_TYPE_QCA9888)) {
        qdf_mem_set(filebuf, sizeof(filebuf), 0);
    } else {
        buflen = get_filenames(target_type,(char*)filebuf, MAX_FILENAMES_SIZE);
        if (buflen <= 0) {
            qdf_info(KERN_ERR "%s: Failed to read board data file list!", __func__);
            qdf_mem_free(filebuf);
            return -1;
        }
    }
    if (boardid_to_filename(soc, board_id, filebuf, buflen, boarddata_file) != 0) {
        qdf_info(KERN_ERR "%s: Failed to determine board data file name!", __func__);
        qdf_mem_free(filebuf);
         /* Made change in boardid_to_filename to fall back to default boarddata file since few boards have Invalid boardID 0 */
        return 0;
    }
   qdf_mem_free(filebuf);
   return 0;
}
qdf_export_symbol(ol_get_board_id);
#endif

void wlan_pdev_set_recovery_inprogress(struct wlan_objmgr_psoc *psoc,
					void *obj, void *set)
{
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_pdev *pdev;
    struct target_pdev_info *pdev_tgt_info;

    pdev = (struct wlan_objmgr_pdev *)obj;

    pdev_tgt_info = wlan_pdev_get_tgt_if_handle(pdev);
    if(pdev_tgt_info == NULL)
       return;
    scn = (struct ol_ath_softc_net80211*)target_pdev_get_feature_ptr(pdev_tgt_info);
    if(scn == NULL)
       return;

    scn->sc_ic.recovery_in_progress = *((uint8_t *)set);
#ifdef QCA_SUPPORT_CP_STATS
    pdev_cp_stats_tgt_asserts_inc(pdev, 1);
#endif

}

void wlan_pdev_acfg_deliver_tgt_assert(struct wlan_objmgr_psoc *psoc,
					void *obj, void *set)
{
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_pdev *pdev;
    struct target_pdev_info *pdev_tgt_info;

    pdev = (struct wlan_objmgr_pdev *)obj;

    pdev_tgt_info = wlan_pdev_get_tgt_if_handle(pdev);
    if(pdev_tgt_info == NULL)
       return;
    scn = (struct ol_ath_softc_net80211*)target_pdev_get_feature_ptr(pdev_tgt_info);
    if(scn == NULL)
       return;

    OSIF_RADIO_DELIVER_EVENT_WATCHDOG(&(scn->sc_ic), *((uint8_t *)set));
}

void
ol_target_failure(void *instance, QDF_STATUS status)
{
    ol_ath_soc_softc_t *soc =  (ol_ath_soc_softc_t *)instance;
    A_UINT32 reg_dump_area = 0;
    A_UINT32 reg_dump_values[REGISTER_DUMP_LEN_MAX];
    A_UINT32 reg_dump_cnt = 0;
    A_UINT32 i;
    A_UINT32 dbglog_hdr_address;
    struct dbglog_hdr_s dbglog_hdr;
    struct dbglog_buf_s dbglog_buf;
    A_UINT8 *dbglog_data;
    struct hif_opaque_softc *hif_hdl;
    extern int hif_dump_ce_registers(struct hif_opaque_softc *);
    uint8_t set;
    uint32_t target_type;
    void *dbglog_handle;
    struct target_psoc_info *tgt_psoc_info;

    target_type =  lmac_get_tgt_type(soc->psoc_obj);

    atomic_inc(&target_assert_count);
    hif_hdl = lmac_get_ol_hif_hdl(soc->psoc_obj);

    if (!hif_hdl) {
        qdf_info("hif handle is NULL: %s ", __func__);
        return;
    }

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
                                                    soc->psoc_obj);
    if (tgt_psoc_info == NULL) {
        qdf_info("%s: target_psoc_info is null ", __func__);
        return;
    }

    if (!(dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info))) {
        qdf_info("%s: dbglog_handle is null ", __func__);
        return;
    }
     set = 1;
     wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                wlan_pdev_set_recovery_inprogress, &set, 0,
		WLAN_MLME_NB_ID);

     soc->recovery_in_progress = 1;

#if UMAC_SUPPORT_ACFG
      set = ACFG_WDT_TARGET_ASSERT;
      wlan_objmgr_iterate_obj_list(soc->psoc_obj, WLAN_PDEV_OP,
                wlan_pdev_acfg_deliver_tgt_assert, &set, 0,
		WLAN_MLME_NB_ID);

#endif

    qdf_info("[%s]: XXX TARGET ASSERTED XXX", soc->sc_osdev->netdev->name);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    qdf_info("<< NSS WIFI OFFLOAD INFORMATION >>>");
    qdf_info("SoC ID %d with NSS ifnum %d ",soc->soc_idx, soc->nss_soc.nss_sifnum);
#endif
    soc->target_status = OL_TRGET_STATUS_RESET;
    /* Per Radio tgt_asserts is incremented above, increment for the SOC here */
    soc->soc_stats.tgt_asserts++;
    if (hif_diag_read_mem(hif_hdl,
                host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_failure_state)),
                (A_UCHAR *)&reg_dump_area,
                sizeof(A_UINT32))!= QDF_STATUS_SUCCESS)
    {
        qdf_info("HifDiagReadiMem FW Dump Area Pointer failed");
        return;
    }

    qdf_info("Target Register Dump Location 0x%08X", reg_dump_area);

    if (target_type == TARGET_TYPE_AR6320) {
        reg_dump_cnt = REG_DUMP_COUNT_AR6320;
    } else  if (target_type == TARGET_TYPE_AR9888) {
        reg_dump_cnt = REG_DUMP_COUNT_AR9888;
    } else  if (target_type == TARGET_TYPE_AR900B || target_type == TARGET_TYPE_IPQ4019) {
        reg_dump_cnt = REG_DUMP_COUNT_AR900B;
    } else  if (target_type == TARGET_TYPE_QCA9984) {
        reg_dump_cnt = REG_DUMP_COUNT_QCA9984;
    } else  if (target_type == TARGET_TYPE_QCA9888) {
        reg_dump_cnt = REG_DUMP_COUNT_QCA9888;
    } else {
        A_ASSERT(0);
    }

    if (hif_diag_read_mem(hif_hdl,
                reg_dump_area,
                (A_UCHAR*)&reg_dump_values[0],
                reg_dump_cnt * sizeof(A_UINT32))!= QDF_STATUS_SUCCESS)
    {
        qdf_info("HifDiagReadiMem for FW Dump Area failed");
        return;
    }

    qdf_info("Target Register Dump");
    for (i = 0; i < reg_dump_cnt; i++) {
        qdf_info("[%02d]   :  0x%08X", i, reg_dump_values[i]);
    }
    /* initial target dump is over, collect the real dumps now */
    /* Schedule a work queue that resets the radio and
       reload the firmware.
    */
    if (soc->pci_reconnect) {
       qdf_info("Resetting  %s radio", soc->sc_osdev->netdev->name);
       soc->pci_reconnect(soc);
    }

    //CE Ring Index 2 (0x00057C00 - 0x00057C54)
    //We are interested only in the address region from 0x00057C34.
#if defined(FIXME_AR900B_CRSH_DUMP)
    qdf_info("CE Registers...");
    sc = hif_hdl;
    if (sc != NULL && sc->mem != NULL) {
        A_UINT32 ce_reg_cnt = 9;
        A_UINT32 ce_reg_start_addr = 0x00057C34;
        A_UINT32 ce_reg_offset = 0;
        A_UINT32 ce_reg_val = 0;
        for (i = 0; i < ce_reg_cnt; i++) {
            ce_reg_val = hif_reg_read(sc, (ce_reg_start_addr + ce_reg_offset));
            qdf_info("[0x%08X]   :  0x%08X", (ce_reg_start_addr + ce_reg_offset), ce_reg_val);
            ce_reg_offset += 4;
        }
    }
#endif

    hif_dump_ce_registers(hif_hdl);

    if (hif_diag_read_mem(hif_hdl,
                host_interest_item_address(target_type, offsetof(struct host_interest_s, hi_dbglog_hdr)),
                (A_UCHAR *)&dbglog_hdr_address,
                sizeof(dbglog_hdr_address))!= QDF_STATUS_SUCCESS)
    {
        qdf_info("HifDiagReadiMem FW dbglog_hdr_address failed");
        return;
    }

    if (hif_diag_read_mem(hif_hdl,
                dbglog_hdr_address,
                (A_UCHAR *)&dbglog_hdr,
                sizeof(dbglog_hdr))!= QDF_STATUS_SUCCESS)
    {
        qdf_info("HifDiagReadiMem FW dbglog_hdr failed");
        return;
    }
    if (hif_diag_read_mem(hif_hdl,
                (A_UINT32)(uintptr_t)dbglog_hdr.dbuf,
                (A_UCHAR *)&dbglog_buf,
                sizeof(dbglog_buf))!= QDF_STATUS_SUCCESS)
    {
        qdf_info("HifDiagReadiMem FW dbglog_buf failed");
        return;
    }

    if(dbglog_buf.length) {
        dbglog_data = qdf_mem_malloc(dbglog_buf.length + 4);
        if (dbglog_data) {

            if (hif_diag_read_mem(hif_hdl,
                        (A_UINT32)(uintptr_t)dbglog_buf.buffer,
                        dbglog_data + 4,
                        dbglog_buf.length)!= QDF_STATUS_SUCCESS)
            {
                qdf_info("HifDiagReadiMem FW dbglog_data failed");
            } else {
                qdf_info("dbglog_hdr.dbuf=%pK dbglog_data=%pK dbglog_buf.buffer=%pK dbglog_buf.length=%u",
                        dbglog_hdr.dbuf, dbglog_data, dbglog_buf.buffer, dbglog_buf.length);

                OS_MEMCPY(dbglog_data, &dbglog_hdr.dropped, 4);
                (void)fwdbg_parse_debug_logs(dbglog_handle, soc, dbglog_data, dbglog_buf.length + 4, NULL);
            }
            qdf_mem_free(dbglog_data);
        }
    } else {
        qdf_info("HifDiagReadiMem FW dbglog_data failed since dbglog_buf.length=%u",dbglog_buf.length);
    }
    return;

}
qdf_export_symbol(ol_target_failure);

#define BMI_EXCHANGE_TIMEOUT_MS  1000

/*
 * Issue a BMI command from a user agent to a Target.
 *
 * Note: A single buffer is used for requests and responses.
 * Synchronization is not enforced by this module.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
static ssize_t
ath_sysfs_BMI_write(struct file *file, struct kobject *kobj,
                   struct bin_attribute *bin_attr,
                   char *buf, loff_t pos, size_t count)
#else
static ssize_t
ath_sysfs_BMI_write(struct kobject *kobj,
                   struct bin_attribute *bin_attr,
                   char *buf, loff_t pos, size_t count)
#endif
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)(bin_attr->private);
    A_UINT32 cmd;
    unsigned int nbytes;
    A_UINT8 *bmi_response_buf;
    u_int32_t *bmi_response_lengthp;
    void *hif_hdl;
    int32_t result;
    struct bmi_info *bmi_handle;

    bmi_handle = soc->bmi_handle;
    hif_hdl = lmac_get_hif_hdl(soc->psoc_obj);
    if (!hif_hdl) {
	   qdf_info("%s-%d: Invalid hif handle", __func__, __LINE__);
	   return A_ERROR;
    }

    nbytes = min(count, (size_t) BMIGetMaxDataSize());
    OS_MEMCPY(bmi_handle->pBMICmdBuf, buf, nbytes); /* NB: buf is in kernel space */
    cmd = *((A_UINT32 *)bmi_handle->pBMICmdBuf); /* peek at command */

    if (BMIIsCmdBmiDone(cmd)) {
        /*
         * Handle BMI_DONE specially -- signal
         * that the BMI user agent is done.
         */
        ol_ath_signal_bmi_user_agent_done(soc);
        return nbytes;
    }

    result = BMIIsCmdResponseExpected(cmd);

    if (result == true) {
        bmi_response_buf = bmi_handle->pBMIRspBuf;
        bmi_response_lengthp = &bmi_handle->last_rxlen;
    } else if (result == false) {
        bmi_response_buf = NULL;
        bmi_response_lengthp = NULL;
    } else {
	   qdf_info(KERN_ERR "BMI sysfs command unknown (%d)\n", cmd);
	   return A_ERROR;
    }

    if (QDF_STATUS_SUCCESS != hif_exchange_bmi_msg(hif_hdl,
                               bmi_handle->BMICmd_pa,
                               bmi_handle->BMIRsp_pa,
                               bmi_handle->pBMICmdBuf,
                               (A_UINT32)nbytes,
                               bmi_response_buf,
                               bmi_response_lengthp,
                               BMI_EXCHANGE_TIMEOUT_MS))

    {
        qdf_info(KERN_ERR "BMI sysfs command failed\n");
        return A_ERROR;
    }

    return nbytes;
}

/*
 * Pass a Target's response back to a user agent.  The response
 * is to a BMI command that was issued earlier through
 * ath_sysfs_BMI_write.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
static ssize_t
ath_sysfs_BMI_read(struct file *file, struct kobject *kobj,
                   struct bin_attribute *bin_attr,
                   char *buf, loff_t pos, size_t count)
#else
static ssize_t
ath_sysfs_BMI_read(struct kobject *kobj,
                   struct bin_attribute *bin_attr,
                   char *buf, loff_t pos, size_t count)
#endif
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)(bin_attr->private);
    unsigned int nbytes;

    nbytes = min((uint32_t)count, soc->bmi_handle->last_rxlen);
    OS_MEMCPY(buf, soc->bmi_handle->pBMIRspBuf, nbytes); /* NB: buf is in kernel space */

    return nbytes;
}

unsigned int
ol_ath_bmi_user_agent_init(ol_ath_soc_softc_t *soc)
{
    int ret;
    struct bin_attribute *BMI_fsattr;

    if (!cfg_get(soc->psoc_obj, CFG_OL_BMI)) {
        return 0; /* User agent not requested */
    }

    soc->bmi_handle->bmiUADone = FALSE;

    BMI_fsattr = OS_MALLOC(soc->sc_osdev, sizeof(*BMI_fsattr), GFP_KERNEL);
    if (!BMI_fsattr) {
        qdf_info("%s: Memory allocation failed\n", __func__);
        return 0;
    }
    OS_MEMZERO(BMI_fsattr, sizeof(*BMI_fsattr));

    BMI_fsattr->attr.name = "bmi";
    BMI_fsattr->attr.mode = 0600;
    BMI_fsattr->read = ath_sysfs_BMI_read;
    BMI_fsattr->write = ath_sysfs_BMI_write;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
    sysfs_bin_attr_init(BMI_fsattr);
#endif
    ret = sysfs_create_bin_file(&soc->sc_osdev->device->kobj, BMI_fsattr);
    if (ret) {
        qdf_info("%s: sysfs create failed\n", __func__);
        OS_FREE(BMI_fsattr);
        return 0;
    }

    BMI_fsattr->private = soc;
    soc->bmi_handle->bmi_ol_priv = (void *)BMI_fsattr;

    return cfg_get(soc->psoc_obj, CFG_OL_BMI);
}
EXPORT_SYMBOL(ol_ath_bmi_user_agent_init);

int
ol_ath_wait_for_bmi_user_agent(ol_ath_soc_softc_t *soc)
{
    int rv;
    struct bin_attribute *BMI_fsattr = (struct bin_attribute *)soc->bmi_handle->bmi_ol_priv;

    rv = wait_event_interruptible(soc->sc_osdev->event_queue, (soc->bmi_handle->bmiUADone));

    sysfs_remove_bin_file(&soc->sc_osdev->device->kobj, BMI_fsattr);
    soc->bmi_handle->bmi_ol_priv = NULL; /* sanity */

    return rv;
}
EXPORT_SYMBOL(ol_ath_wait_for_bmi_user_agent);

void
ol_ath_signal_bmi_user_agent_done(ol_ath_soc_softc_t *soc)
{
    soc->bmi_handle->bmiUADone = TRUE;
    wake_up(&soc->sc_osdev->event_queue);
}
EXPORT_SYMBOL(ol_ath_signal_bmi_user_agent_done);

#endif
