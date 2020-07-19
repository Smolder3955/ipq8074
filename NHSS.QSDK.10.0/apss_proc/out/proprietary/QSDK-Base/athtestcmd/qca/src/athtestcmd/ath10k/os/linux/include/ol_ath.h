#ifndef __OL_ATH_H__
#define __OL_ATH_H__

#define BUS_TYPE_SIM   0x1 /* simulator */
#define BUS_TYPE_PCIE  0x2 /* pcie */
#define BUS_TYPE_SDIO  0x3 /* sdio */
#define BUS_TYPE_AHB   0x4 /* ahb */

struct ol_attach_t {
    u_int16_t devid;
    u_int16_t bus_type;
    u_int32_t target_type;
};

/* Temp defines */
#define AR6004_REV1_VERSION                 0x30000623
#define AR6004_REV1_FIRMWARE_FILE           "athwlan.bin"
#define AR6004_REV1_UTF_FIRMWARE_FILE       "utf.bin"
#define AR6004_REV1_BOARD_DATA_FILE         "fakeBoardData_AR6004.bin"
#define AR6004_REV1_EPPING_FIRMWARE_FILE    "endpointping.bin"
#define AR6004_REV1_OTP_FILE                 "otp.bin"
#define AR6004_VERSION_REV1_3               0x31c8088a
#define AR6004_REV1_3_FIRMWARE_FILE         L"ar6004v1_1fw.bin"
#define AR6004_REV1_3_BOARD_DATA_FILE       L"FakeBoardData.bin"

/* AR9888 Revision 1 */
#define AR9888_REV1_VERSION                 0x4000002c /* Build#44 */
#define AR9888_REV1_FIRMWARE_FILE           "AR9888/hw.1/athwlan.bin"
#define AR9888_REV1_UTF_FIRMWARE_FILE       "AR9888/hw.1/utf.bin"
#define AR9888_REV1_BOARD_DATA_FILE         "AR9888/hw.1/fakeBoardData_AR6004.bin"
#define AR9888_REV1_OTP_FILE                "AR9888/hw.1/otp.bin"

/* AR9888 Revision 2 */
#define AR9888_REV2_VERSION                 0x4100016c /* __VER_MINOR_ 1, __BUILD_NUMBER_ 364 */
#define AR9888_REV2_FIRMWARE_FILE           "AR9888/hw.2/athwlan.bin"
#if QCA_LTEU_SUPPORT
#define AR9888_REV2_FIRMWARE_FILE_LTEU      "AR9888/hw.2/athwlan_lteu.bin"
#endif
#define AR9888_REV2_UTF_FIRMWARE_FILE       "AR9888/hw.2/utf.bin"
#define AR9888_REV2_BOARD_DATA_FILE         "AR9888/hw.2/fakeBoardData_AR6004.bin"
#define AR9888_REV2_OTP_FILE                "AR9888/hw.2/otp.bin"

/* AR9888 Developer version */
#define AR9888_DEV_VERSION                  0x4100270f /* __VER_MINOR_ 1, __BUILD_NUMBER_ 9999 */
#define AR9888_DEV_FIRMWARE_FILE            "athwlan.bin"
#define AR9888_DEV_UTF_FIRMWARE_FILE        "utf.bin"
#define AR9888_DEV_BOARD_DATA_FILE          "fakeBoardData_AR6004.bin"
#define AR9888_DEV_OTP_FILE                 "otp.bin"

/* AR9887 Revision 1 */
#define AR9887_REV1_VERSION                 0x4100016d
#define AR9887_REV1_FIRMWARE_FILE           "AR9887/athwlan.bin"
#define AR9887_REV1_UTF_FIRMWARE_FILE       "AR9887/utf.bin"
#define AR9887_REV1_BOARD_DATA_FILE         "AR9887/fakeBoardData_AR6004.bin"
#define AR9887_REV1_OTP_FILE                "AR9887/otp.bin"
#define AR9887_REV1_BOARDDATA_FILE          "AR9887/boarddata_0.bin"

#define AR6320_REV1_VERSION                 0x4000280F
#define AR6320_REV1_FIRMWARE_FILE           "athwlan.bin"
#define AR6320_REV1_UTF_FIRMWARE_FILE       "utf.bin"
#define AR6320_REV1_BOARD_DATA_FILE         "fakeBoardData_AR6004.bin"

#define AR900B_DEV_VERSION                  0x1000000
#define AR900B_VER1_OTP_FILE                "AR900B/hw.1/otp.bin"
#define AR900B_VER2_OTP_FILE                "AR900B/hw.2/otp.bin"
#define AR900B_VER1_FIRMWARE_FILE           "AR900B/hw.1/athwlan.bin"
#define AR900B_VER2_FIRMWARE_FILE           "AR900B/hw.2/athwlan.bin"
#define AR900B_VER1_UTF_FIRMWARE_FILE       "AR900B/hw.1/utf.bin"
#define AR900B_VER2_UTF_FIRMWARE_FILE       "AR900B/hw.2/utf.bin"
#define AR900B_VER1_PATH                    "AR900B/hw.1/"
#define AR900B_VER2_PATH                    "AR900B/hw.2/"

#define QCA9984_DEV_VERSION                 0x1000000
#define QCA9984_HW_VER1_OTP_FILE            "QCA9984/hw.1/otp.bin"
#define QCA9984_HW_VER1_FIRMWARE_FILE       "QCA9984/hw.1/athwlan.bin"
#define QCA9984_HW_VER1_UTF_FIRMWARE_FILE   "QCA9984/hw.1/utf.bin"
#define QCA9984_HW_VER1_PATH                "QCA9984/hw.1/"
#define QCA9984_M2M_VER1_OTP_FILE           "QCA9984/fpga.1/M2M/otp.bin"
#define QCA9984_M2M_VER1_FIRMWARE_FILE      "QCA9984/fpga.1/M2M/athwlan.bin"
#define QCA9984_M2M_VER1_UTF_FIRMWARE_FILE  "QCA9984/fpga.1/M2M/utf.bin"
#define QCA9984_M2M_VER1_PATH               "QCA9984/fpga.1/M2M/"
#define QCA9984_BB_VER1_OTP_FILE            "QCA9984/fpga.1/BB/otp.bin"
#define QCA9984_BB_VER1_FIRMWARE_FILE       "QCA9984/fpga.1/BB/athwlan.bin"
#define QCA9984_BB_VER1_UTF_FIRMWARE_FILE   "QCA9984/fpga.1/BB/utf.bin"
#define QCA9984_BB_VER1_PATH                "QCA9984/fpga.1/BB/"

/* IPQ4019 Developer version */
#define IPQ4019_DEV_VERSION                 0x1000000
#define IPQ4019_HW_VER1_OTP_FILE            "IPQ4019/hw.1/otp.bin"
#define IPQ4019_HW_VER1_FIRMWARE_FILE       "IPQ4019/hw.1/athwlan.bin"
#define IPQ4019_HW_VER1_UTF_FIRMWARE_FILE   "IPQ4019/hw.1/utf.bin"
#define IPQ4019_HW_VER1_PATH                "IPQ4019/hw.1/"
#define IPQ4019_M2M_VER1_OTP_FILE           "IPQ4019/fpga.1/M2M/otp.bin"
#define IPQ4019_M2M_VER1_FIRMWARE_FILE      "IPQ4019/fpga.1/M2M/athwlan.bin"
#define IPQ4019_M2M_VER1_UTF_FIRMWARE_FILE  "IPQ4019/fpga.1/M2M/utf.bin"
#define IPQ4019_M2M_VER1_PATH               "IPQ4019/fpga.1/M2M/"
#define IPQ4019_BB_VER1_OTP_FILE            "IPQ4019/fpga.1/BB/otp.bin"
#define IPQ4019_BB_VER1_FIRMWARE_FILE       "IPQ4019/fpga.1/BB/athwlan.bin"
#define IPQ4019_BB_VER1_UTF_FIRMWARE_FILE   "IPQ4019/fpga.1/BB/utf.bin"
#define IPQ4019_BB_VER1_PATH                "IPQ4019/fpga.1/BB/"
#define IPQ4019_HW_VER1_RADIO0_CALDATA_FILE "IPQ4019/hw.1/caldata_0.bin"
#define IPQ4019_HW_VER1_RADIO1_CALDATA_FILE "IPQ4019/hw.1/caldata_1.bin"
#define IPQ4019_DK01_2G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_DK01_2G.bin"
#define IPQ4019_DK01_5G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_DK01_5G.bin"
#define IPQ4019_DK03_2G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_DK03_wifi0.bin"
#define IPQ4019_DK03_5G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_DK03_wifi1.bin"
#define IPQ4019_DK04_2G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_DK04_2G.bin"
#define IPQ4019_DK04_5G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_DK04_5G.bin"
#define IPQ4019_DK04_NEGATIVE_POWER_2G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_DK04_2G_neg_pwr.bin"
#define IPQ4019_DK04_NEGATIVE_POWER_5G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_DK04_5G_neg_pwr.bin"
#define IPQ4019_DK01_Y9803_2G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_Y9803_wifi0.bin"
#define IPQ4019_DK01_Y9803_5G_BOARD_DATA_FILE     "boardData_1_0_IPQ4019_Y9803_wifi1.bin"

typedef enum {
    DK01_2G_Y9803 = 0x10,
    DK01_5G_Y9803,
    DK03_2G,
    DK03_5G,
    DK04_2G,
    DK04_5G,
    DK04_NEGATIVE_POWER_2G,
    DK04_NEGATIVE_POWER_5G,
} IPQ4019_BOARD_IDS;

#define QCA9888_DEV_VERSION                 0x1000000
#define QCA9888_HW_VER1_OTP_FILE            "QCA9888/hw.1/otp.bin"
#define QCA9888_HW_VER1_FIRMWARE_FILE       "QCA9888/hw.1/athwlan.bin"
#define QCA9888_HW_VER1_UTF_FIRMWARE_FILE   "QCA9888/hw.1/utf.bin"
#define QCA9888_HW_VER1_PATH                "QCA9888/hw.1/"
#define QCA9888_M2M_VER1_OTP_FILE           "QCA9888/fpga.1/M2M/otp.bin"
#define QCA9888_M2M_VER1_FIRMWARE_FILE      "QCA9888/fpga.1/M2M/athwlan.bin"
#define QCA9888_M2M_VER1_UTF_FIRMWARE_FILE  "QCA9888/fpga.1/M2M/utf.bin"
#define QCA9888_M2M_VER1_PATH               "QCA9888/fpga.1/M2M/"
#define QCA9888_BB_VER1_OTP_FILE            "QCA9888/fpga.1/BB/otp.bin"
#define QCA9888_BB_VER1_FIRMWARE_FILE       "QCA9888/fpga.1/BB/athwlan.bin"
#define QCA9888_BB_VER1_UTF_FIRMWARE_FILE   "QCA9888/fpga.1/BB/utf.bin"
#define QCA9888_BB_VER1_PATH                "QCA9888/fpga.1/BB/"
#define QCA9888_Y9582_BOARD_DATA_FILE       "boardData_1_0_QCA9888_5G_Y9582.bin"
#define QCA9888_Y9484_BOARD_DATA_FILE       "boardData_1_0_QCA9888_5G_Y9484.bin"
#define QCA9888_Y9690_BOARD_DATA_FILE       "boardData_1_0_QCA9888_5G_Y9690.bin"
#define QCA9888_Y9582_Y_BOARD_DATA_FILE     "boardData_1_0_QCA9888_5G_Y9582_Y.bin"
#define QCA9888_Y9484_Y_BOARD_DATA_FILE     "boardData_1_0_QCA9888_5G_Y9484_Y.bin"
#define QCA9888_Y9690_Y_BOARD_DATA_FILE     "boardData_1_0_QCA9888_5G_Y9690_Y.bin"

typedef enum {
    QCA9888_Y9690 = 0x0A,
    QCA9888_Y9484,
    QCA9888_Y9582,
    QCA9888_Y9690_Y,
    QCA9888_Y9484_Y,
    QCA9888_Y9582_Y,
} QCA9888_BOARD_IDS;

/* ARXXXX: all developing chips which are valid but won't match others above */
#define ARXXXX_DEV_FIRMWARE_FILE            "athwlan.bin"
#define ARXXXX_DEV_UTF_FIRMWARE_FILE        "utf.bin"
#define ARXXXX_DEV_BOARD_DATA_FILE          "fakeBoardData_AR6004.bin"
#define ARXXXX_DEV_OTP_FILE                 "otp.bin"

/* Configuration for statistics pushed by firmware */
#define PDEV_DEFAULT_STATS_UPDATE_PERIOD    500
#define VDEV_DEFAULT_STATS_UPDATE_PERIOD    500
#define PEER_DEFAULT_STATS_UPDATE_PERIOD    500

#ifdef CONFIG_AR900B_SUPPORT
/*
 * Total 7 Target Section Dump
 */
#define FW_DRAM_ADDRESS	    0x00400000

#define FW_DRAM_LENGTH_AR900B	0x00060000 /* 384 K */
#define FW_DRAM_LENGTH_IPQ4019	0x00068000 /* 416*/
#define FW_DRAM_LENGTH_AR9888	0x00050000 /* 320 K */
#define FW_DRAM_LENGTH_QCA9984	0x00080000 /* 524 K */

#define FW_IO_MEM_ADDR_AR900B	0x44000000
#define FW_IO_MEM_ADDR_IPQ4019	0x87500000
#define FW_IO_MEM_ADDR_AR9888   0x41400000

#define FW_IO_MEM_SIZE_AR900B     (6 << 20)/* 6M  */
#define FW_IO_MEM_SIZE_IPQ4019     (6 << 20)/* 6M  */
#define FW_IO_MEM_SIZE_AR9888     ((1 * 1024 * 1024) - 128)

#define FW_IRAM_ADDRESS     0x00980000
#define FW_IRAM_LENGTH      0x00050000 /* 320 K */
#define FW_SRAM_ADDRESS     0x000C0000
#define FW_SRAM_LENGTH      0x00040000 /* 256 K */

#define HW_APB_REG_SPACE    0x00030000 /* MAC, WifiCmn, WifiTimers */
#define HW_APB_REG_LENGTH   0x00007000 /* MAC, WifiCmn, WifiTimers NOTE: TXPCU BUF not readable*/

#define HW_APB_REG_SPACE1   0x0003f000 /* MAC HW_SCH, CRYPTO, MXI. */
#define HW_APB_REG_LENGTH1  0x00003000

#define HW_WIFI_REG_SPACE   0x00043000 /* Wifi Coex, Wifi RTC */
#define HW_WIFI_REG_LENGTH  0x00003000 /* Wifi Coex, Wifi RTC */

#define HW_CE_REG_SPACE     0x0004A000 /* Copy Engine */
#define HW_CE_REG_LENGTH    0x00005000 /* Copy Engine */

#define SOC_REG_SPACE       0x00080000 /* SoC RTC, PCie, Core */
#define SOC_REG_LENGTH      0x00006000 /* SoC RTC, PCie, Core */
#endif

/* Defualt boarddata file names that needs to be loaded when Invalid boardid is returned*/
#if DEFAULT_BOARD_DATA
#define DEFAULT_BOARDDATA_FILE_5G "boarddata_0.bin"
#define DEFAULT_BOARDDATA_FILE_2G "boarddata_1.bin"
#endif

/* Platform specific configuration for max. no. of fragments */
#define QCA_OL_11AC_TX_MAX_FRAGS            2

int __ol_ath_attach(hif_softc_t hif_sc, struct ol_attach_t *cfg, osdev_t osdev, struct net_device **pdev);
void __ol_ath_suspend_resume_attach(struct net_device *dev);
int __ol_ath_detach(struct net_device *dev);
int __ol_ath_suspend(struct net_device *dev);
int __ol_ath_resume(struct net_device *dev);
void __ol_ath_target_status_update(struct net_device *dev, ol_target_status status);
int __ol_vap_delete_on_rmmod(struct net_device *dev);

#if QCA_LTEU_SUPPORT
extern unsigned int lteu_support;
#endif

#ifdef HIF_PCI
#if !defined(A_SIMOS_DEVHOST)
#define CONFIG_ATH_SYSFS_DIAG_SUPPORT /* include user-level diagnostic support? */
#endif

int ol_ath_pci_configure(hif_softc_t hif_sc, struct net_device *dev, hif_handle_t *hif_hdl);
void ol_ath_pci_nointrs(struct net_device *dev);
#elif defined(HIF_SDIO)
#if !defined(A_SIMOS_DEVHOST)
#define CONFIG_ATH_SYSFS_DIAG_SUPPORT /* include user-level diagnostic support? */
#endif
int ol_ath_sdio_configure(hif_softc_t hif_sc, struct net_device *dev, hif_handle_t *hif_hdl);
#endif
#ifdef ATH_AHB
int ol_ath_ahb_configure(hif_softc_t hif_sc, struct net_device *dev, hif_handle_t *hif_hdl);
#endif

void ol_scn_is_target_ar900b(struct ol_ath_softc_net80211 *scn); 

#endif /* __OL_ATH_H__ */

