/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * $ATH_LICENSE_HOSTSDK0_C$
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <linux/version.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <a_osapi.h>
#include <athdefs.h>
#include <athtypes_linux.h>

#include "./mac_mcmn_reg.h"
#include "./mac_tracer.h"
#include "./beeliner_eventNames.h"
#include "./wmac_debug_select.h"
#include "./phydbg_adc_capture.h"
#include "./beeliner_event_select.h"
#include "./bb_sm_recorder.h"

#if defined(AR900B)

#include "apb_map.h"
#include "rtc_soc_reg.h"
#include "efuse_reg.h"

const A_UINT32 REG_RTCSOC_OTP =        OTP_ADDRESS;
const A_UINT32 REG_RTCSOC_OTP_STATUS = OTP_STATUS_ADDRESS;
const A_UINT32 REG_EFUSE_PG_STROBE_PW = PG_STROBE_PW_REG_ADDRESS;
const A_UINT32 REG_EFUSE_RD_STROBE_PW = RD_STROBE_PW_REG_ADDRESS;
const A_UINT32 REG_EFUSE_WR_ENABLE = EFUSE_WR_ENABLE_REG_ADDRESS;
const A_UINT32 REG_EFUSE_VDDQ_SETTLE_TIME = VDDQ_SETTLE_TIME_REG_ADDRESS;
const A_UINT32 REG_EFUSE_BITMASK_WR = BITMASK_WR_REG_ADDRESS;
const A_UINT32 REG_EFUSE_INTF0 = EFUSE_INTF0_ADDRESS;

#else

#include "hw/apb_athr_wlan_map.h"
#include "hw/rtc_soc_reg.h"
#include "hw/efuse_reg.h"
const A_UINT32 REG_RTCSOC_OTP =        RTC_SOC_BASE_ADDRESS + OTP_OFFSET;
const A_UINT32 REG_RTCSOC_OTP_STATUS = RTC_SOC_BASE_ADDRESS + OTP_STATUS_OFFSET;
const A_UINT32 REG_EFUSE_PG_STROBE_PW = EFUSE_BASE_ADDRESS + PG_STROBE_PW_REG_OFFSET;
const A_UINT32 REG_EFUSE_RD_STROBE_PW = EFUSE_BASE_ADDRESS + RD_STROBE_PW_REG_OFFSET;
const A_UINT32 REG_EFUSE_WR_ENABLE = EFUSE_BASE_ADDRESS+EFUSE_WR_ENABLE_REG_OFFSET;
const A_UINT32 REG_EFUSE_VDDQ_SETTLE_TIME = EFUSE_BASE_ADDRESS+VDDQ_SETTLE_TIME_REG_OFFSET;
const A_UINT32 REG_EFUSE_BITMASK_WR = EFUSE_BASE_ADDRESS + BITMASK_WR_REG_OFFSET;
const A_UINT32 REG_EFUSE_INTF0 = EFUSE_BASE_ADDRESS + EFUSE_INTF0_OFFSET;

#endif


#include "qc98xx_eeprom.h"
#include "Qc98xxEepromStruct.h"
#include "rate_constants.h"
#include "vrate_constants.h"
#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/*
 * This is a user-level agent which provides diagnostic read/write
 * access to Target space.  This may be used
 *   to collect information for analysis
 *   to read/write Target registers
 *   etc.
 */

#define DIAG_READ_TARGET      1
#define DIAG_WRITE_TARGET     2
#define DIAG_READ_WORD        3
#define DIAG_WRITE_WORD       4
#define DIAG_PHYDBG_DUMP      5
#define DIAG_TRACE_DUMP       6
#define DIAG_EVENT_CFG        7
#define DIAG_TRACE_START      8
#define DIAG_TRACE_STOP       9
#define DIAG_TRACE_CLEAR     10
#define DIAG_TRACE_CFG       11
#define DIAG_CONFIG_PHYDBG_ADCCAPTURE		12
#define DIAG_DUMP_PHYDBG_ADCCAPTURE		13
#define DIAG_PHYDBG_STOP			14
#define DIAG_BB_SM_RECORDER_CFG			15
#define DIAG_BB_SM_RECORDER_DUMP		16
#define DIAG_PHYTLV_CNT                 17
#define DIAG_PHYERR_CNT                 18
#define DIAG_CYCLE_CNT                  19


#define ADDRESS_FLAG                    0x001
#define LENGTH_FLAG                     0x002
#define PARAM_FLAG                      0x004
#define FILE_FLAG                       0x008
#define UNUSED0x010                     0x010
#define AND_OP_FLAG                     0x020
#define BITWISE_OP_FLAG                 0x040
#define QUIET_FLAG                      0x080
#define OTP_FLAG                        0x100
#define HEX_FLAG                     	0x200 //dump file mode,x: hex mode; other binary mode.
#define UNUSED0x400                     0x400
#define DEVICE_FLAG                     0x800
#define WORDCOUNT_FLAG                  0x1000
#define INDEX_FLAG                  0x2000
#define EVENTDATAMASK_FLAG                  0x4000
#define EVENTDATAVALUE_FLAG                  0x8000

//eventbus config file parser flags
#define EVENTCFG_BEGIN_FLAG 0x001
#define EVENTCFG_END_FLAG 0xe
#define EVENTSTOP_BEGIN_FLAG 0x002
#define EVENTSTOP_END_FLAG 0xd
#define EVENTSTOPDATA_BEGIN_FLAG 0x004
#define EVENTSTOPDATA_END_FLAG 0xb
#define BUFFER_CFG_BEGIN_FLAG 0x008
#define BUFFER_CFG_END_FLAG 0x7
/* Limit malloc size when reading/writing file */
#define MAX_BUF                         (8*1024)

#define MBUFFER 1024
#define STRCAT_BUF(buf1, buf2) do {                                            \
    if ((strlen(buf1) + strlen(buf2)) < (MBUFFER+2)) {                         \
        strcat(buf1, buf2);                                                    \
    }                                                                          \
    else                                                                       \
        printf("Buffer size is too less to perform string concatenation\n");   \
} while(0)




FILE *pFile;
unsigned int flag;
unsigned int StopOnEventDataFlag;
unsigned int StopOnEventIdxFlag;
const char *progname;
const char commands[] =
"commands and options:\n\
--get --address=<target word address> [--count=<wordcount>]\n\
--set --address=<target word address> --[value|param]=<value>\n\
                                      --or=<OR-ing value>\n\
                                      --and=<AND-ing value>\n\
--read --address=<target address> --length=<bytes> --file=<filename>\n\
--write --address=<target address> --file=<filename>\n\
                                   --[value|param]=<value>\n\
--otp --read --address=<otp offset> --length=<bytes> --file=<filename>\n\
--otp --write --address=<otp offset> --file=<filename>\n\
--tracerClear\n\
--eventCfg --file=<event_config_filename>\n\
--tracerCfg --file=<tracer_config_filename>\n\
--tracerStart\n\
--tracerStop\n\
--tracerDump --file=<filename>\n\
--smrecorderCfg\n\
--smrecorderDump --file=<filename>\n\
--phydbgCfg --file=<phydbg_cfg_filename\n\
--phydbgDump --file=<phydbg_cfg_filename\n\
--phydbgStop\n\
--quiet\n\
--device=<device name> (if not default)\n\
--phytlv_cnt\n\
--phyerr_cnt\n\
--cycle_cnt\n\
The options can also be given in the abbreviated form --option=x or -o x.\n\
The options can be given in any order.\n\
If you need to read cal data add --cal ahead of --read command";

#define A_ROUND_UP(x, y)             ((((x) + ((y) - 1)) / (y)) * (y))

#define quiet() (flag & QUIET_FLAG)
#define nqprintf(args...) if (!quiet()) {printf(args);}
#define min(x,y) ((x) < (y) ? (x) : (y))
#define rev(ui) ((ui >> 24) |((ui<<8) & 0x00FF0000) | ((ui>>8) & 0x0000FF00) | (ui << 24))
#define rev2(a) ((a>>8)|((a&0xff)<<8))
void ReadTargetRange(int dev, A_UINT32 address, A_UINT8 *buffer, A_UINT32 length);
void ReadTargetWord(int dev, A_UINT32 address, A_UINT32 *buffer);
void WriteTargetRange(int dev, A_UINT32 address, A_UINT8 *buffer, A_UINT32 length);
void WriteTargetWord(int dev, A_UINT32 address, A_UINT32 value);
int ValidWriteOTP(int dev, A_UINT32 address, A_UINT8 *buffer, A_UINT32 length);
void print_cnt(int dev, A_UINT32 address);

int Qc98xxTargetPowerGet(int frequency, int rate, double *power);

INLINE void *
MALLOC(int nbytes)
{
    void *p= malloc(nbytes);

    if (!p)
    {
        fprintf(stderr, "err -Cannot allocate memory\n");
    }

    return p;
}

void
usage(void)
{
    fprintf(stderr, "usage:\n%s ", progname);
    fprintf(stderr, "%s\n", commands);
    exit(-1);
}
A_UINT8 alphaThermChans2G[QC98XX_NUM_ALPHATHERM_CHANS_2G] = {
    WHAL_FREQ2FBIN(2412, 1), WHAL_FREQ2FBIN(2437,1), WHAL_FREQ2FBIN(2457,1), WHAL_FREQ2FBIN(2472,1)
};

A_UINT8 alphaThermChans5G[QC98XX_NUM_ALPHATHERM_CHANS_5G] = {
    WHAL_FREQ2FBIN(5200,0), WHAL_FREQ2FBIN(5300,0), WHAL_FREQ2FBIN(5520,0), WHAL_FREQ2FBIN(5580,0),
    WHAL_FREQ2FBIN(5640,0), WHAL_FREQ2FBIN(5700,0), WHAL_FREQ2FBIN(5765,0), WHAL_FREQ2FBIN(5805,0)
};

void
ReadTargetRange(int dev, A_UINT32 address, A_UINT8 *buffer, A_UINT32 length)
{
    int nbyte;
    unsigned int remaining;

    (void)lseek(dev, address, SEEK_SET);

    remaining = length;
    while (remaining) {
        nbyte = read(dev, buffer, (size_t)remaining);
        if (nbyte <= 0) {
            fprintf(stderr, "err %s failed (nbyte=%d, address=0x%lx remaining=%d).\n",
                __FUNCTION__, nbyte, (long unsigned int)address, remaining);
            exit(1);
        }

        remaining -= nbyte;
        buffer += nbyte;
        address += nbyte;
    }
}

void
ReadTargetWord(int dev, A_UINT32 address, A_UINT32 *buffer)
{
    ReadTargetRange(dev, address, (A_UINT8 *)buffer, sizeof(*buffer));
}

void
InitializeOtpTimingParameters(int dev)
{
#if (AR900B)
    {
        // Hard-coded for worst case of 148MHz
        const A_UINT32 DESIRED_VDDQ_SETTLE_TIME_CLK = 0x30;
        const A_UINT32 DESIRED_VDDQ_HOLD_TIME_CLK = 0x1;
        const A_UINT32 DESIRED_RD_STROBE_PULSE_WIDTH_CLK = 0x6;
        const A_UINT32 DESIRED_PG_STROBE_PULSE_WIDTH_CLK = 0x58d;
        const A_UINT32 DESIRED_PGENB_SETUP_HOLD_TIME_CLK = 0x1;
        const A_UINT32 DESIRED_STROBE_PULSE_INTERVAL_CLK = 0x1;
        const A_UINT32 DESIRED_CSB_ADDR_LOAD_SETUP_HOLD_CLK = 0x1;
        const A_UINT32 DESIRED_SUR_PD_CSB_CLK = 0x48;
        const A_UINT32 DESIRED_SUP_PS_CSB_CLK = 0x8;

        A_UINT32 dummy_read;

        WriteTargetWord(dev, VDDQ_SETTLE_TIME_REG_ADDRESS, VDDQ_SETTLE_TIME_REG_V_SET(DESIRED_VDDQ_SETTLE_TIME_CLK));
        WriteTargetWord(dev, VDDQ_HOLD_TIME_REG_ADDRESS, VDDQ_HOLD_TIME_REG_V_SET(DESIRED_VDDQ_HOLD_TIME_CLK));

        WriteTargetWord(dev, RD_STROBE_PW_REG_ADDRESS, RD_STROBE_PW_REG_V_SET(DESIRED_RD_STROBE_PULSE_WIDTH_CLK));
        WriteTargetWord(dev, PG_STROBE_PW_REG_ADDRESS, PG_STROBE_PW_REG_V_SET(DESIRED_PG_STROBE_PULSE_WIDTH_CLK));
        WriteTargetWord(dev, PGENB_SETUP_HOLD_TIME_REG_ADDRESS, PGENB_SETUP_HOLD_TIME_REG_V_SET(DESIRED_PGENB_SETUP_HOLD_TIME_CLK));
        WriteTargetWord(dev, STROBE_PULSE_INTERVAL_REG_ADDRESS, STROBE_PULSE_INTERVAL_REG_V_SET(DESIRED_STROBE_PULSE_INTERVAL_CLK));
        WriteTargetWord(dev, CSB_ADDR_LOAD_SETUP_HOLD_REG_ADDRESS, CSB_ADDR_LOAD_SETUP_HOLD_REG_V_SET(DESIRED_CSB_ADDR_LOAD_SETUP_HOLD_CLK));

        WriteTargetWord(dev, SUR_PD_CSB_REG_ADDRESS, SUR_PD_CSB_REG_V_SET(DESIRED_SUR_PD_CSB_CLK));
        WriteTargetWord(dev, SUP_PS_CSB_REG_ADDRESS, SUP_PS_CSB_REG_V_SET(DESIRED_SUP_PS_CSB_CLK));

        ReadTargetWord(dev, PG_STROBE_PW_REG_ADDRESS, &dummy_read);/*  flush  */
    }
#else
    /* Conservatively set OTP read/write timing for 110MHz core clock */
    WriteTargetWord(dev, REG_EFUSE_VDDQ_SETTLE_TIME, 2200);
    WriteTargetWord(dev, REG_EFUSE_PG_STROBE_PW, 605);
    WriteTargetWord(dev, REG_EFUSE_RD_STROBE_PW, 6);
#endif
}

void
ReadTargetOTP(int dev, A_UINT32 offset, A_UINT8 *buffer, A_UINT32 length)
{
    A_UINT32 status_mask;
    A_UINT32 otp_status;
    int i;

    /* Enable OTP reads */

    WriteTargetWord(dev, REG_RTCSOC_OTP, OTP_VDD12_EN_SET(1));

    status_mask = OTP_STATUS_VDD12_EN_READY_SET(1);
    do {
        ReadTargetWord(dev, REG_RTCSOC_OTP_STATUS, &otp_status);
    } while ((otp_status & OTP_STATUS_VDD12_EN_READY_MASK) != status_mask);

    InitializeOtpTimingParameters(dev);

    /* Read data from OTP */
    for (i=0; i<length; i++, offset++) {
        A_UINT32 efuse_word;

        ReadTargetWord(dev, REG_EFUSE_INTF0 + (offset<<2), &efuse_word);
        buffer[i] = (A_UINT8)efuse_word;
    }

    /* Disable OTP */
    WriteTargetWord(dev, REG_RTCSOC_OTP, 0);
}

void
WriteTargetRange(int dev, A_UINT32 address, A_UINT8 *buffer, A_UINT32 length)
{
    int nbyte;
    unsigned int remaining;

    (void)lseek(dev, address, SEEK_SET);

    remaining = length;
    while (remaining) {
        nbyte = write(dev, buffer, (size_t)remaining);
        if (nbyte <= 0) {
            fprintf(stderr, "err %s failed (nbyte=%d, address=0x%lx remaining=%d).\n",
                __FUNCTION__, nbyte, (long unsigned int)address, remaining);
        }

        remaining -= nbyte;
        buffer += nbyte;
        address += nbyte;
    }
}

void
WriteTargetWord(int dev, A_UINT32 address, A_UINT32 value)
{
    A_UINT32 param = value;

    WriteTargetRange(dev, address, (A_UINT8 *)&param, sizeof(param));
}

#define BAD_OTP_WRITE(have, want) ((((have) ^ (want)) & (have)) != 0)

/*
 * Check if the current contents of OTP and the desired
 * contents specified by buffer/length are compatible.
 * If we're trying to CLEAR an OTP bit, then this request
 * is invalid.
 * returns: 0-->INvalid; 1-->valid
 */
int
ValidWriteOTP(int dev, A_UINT32 offset, A_UINT8 *buffer, A_UINT32 length)
{
    int i;
    A_UINT8 *otp_contents = NULL;

    otp_contents = MALLOC(length);
    if (otp_contents == NULL) {
        /* Malloc failed */
        return 0;
    }
    ReadTargetOTP(dev, offset, otp_contents, length);

    for (i=0; i<length; i++) {
        if (BAD_OTP_WRITE(otp_contents[i], buffer[i])) {
            fprintf(stderr, "Abort. Cannot change offset %u from 0x%02x to 0x%02x\n",
                offset+i, otp_contents[i], buffer[i]);
            return 0;
        }
    }

    return 1;
}
void PrintQc98xxBaseHeader(int client, const BASE_EEP_HEADER *pBaseEepHeader, const QC98XX_EEPROM *pEeprom)
{
    char  buffer[MBUFFER];

    SformatOutput(buffer, MBUFFER-1, "| RegDomain 1              0x%04X   |  RegDomain 2             0x%04X   |",
        pBaseEepHeader->regDmn[0],
        pBaseEepHeader->regDmn[1]
        );
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1, "| TX Mask                  0x%X      |  RX Mask                 0x%X      |",
        (pBaseEepHeader->txrxMask&0xF0)>>4,
        pBaseEepHeader->txrxMask&0x0F
        );
	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1,"| OpFlags: 5GHz            %d        |  2GHz                    %d        |",
             (pBaseEepHeader->opCapBrdFlags.opFlags & WHAL_OPFLAGS_11A) || 0,
             (pBaseEepHeader->opCapBrdFlags.opFlags & WHAL_OPFLAGS_11G) || 0
             );
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1,"| 5G HT20                  %d        |  2G HT20                 %d        |",
              (pBaseEepHeader->opCapBrdFlags.opFlags & WHAL_OPFLAGS_5G_HT20) || 0,
              (pBaseEepHeader->opCapBrdFlags.opFlags & WHAL_OPFLAGS_2G_HT20) || 0
                );
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1,"| 5G HT40                  %d        |  2G HT40                 %d        |",
              (pBaseEepHeader->opCapBrdFlags.opFlags & WHAL_OPFLAGS_5G_HT40) || 0,
              (pBaseEepHeader->opCapBrdFlags.opFlags & WHAL_OPFLAGS_2G_HT40) || 0
                );
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1,"| 5G VHT20                 %d        |  2G VHT20                %d        |",
              (pBaseEepHeader->opCapBrdFlags.opFlags2 & WHAL_OPFLAGS2_5G_VHT20) || 0,
              (pBaseEepHeader->opCapBrdFlags.opFlags2 & WHAL_OPFLAGS2_2G_VHT20) || 0
                );
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1,"| 5G VHT40                 %d        |  2G VHT40                %d        |",
              (pBaseEepHeader->opCapBrdFlags.opFlags2 & WHAL_OPFLAGS2_5G_VHT40) || 0,
              (pBaseEepHeader->opCapBrdFlags.opFlags2 & WHAL_OPFLAGS2_2G_VHT40) || 0
                );
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1,"| 5G VHT80                 %d        |                                   |",
              (pBaseEepHeader->opCapBrdFlags.opFlags2 & WHAL_OPFLAGS2_5G_VHT80) || 0
                );
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1,"| Big Endian               %d        |  Wake On Wireless        %d        |",
             (pBaseEepHeader->opCapBrdFlags.miscConfiguration & WHAL_MISCCONFIG_EEPMISC_BIG_ENDIAN) || 0, (pBaseEepHeader->opCapBrdFlags.miscConfiguration & WHAL_MISCCONFIG_EEPMISC_WOW) || 0);

  	fprintf(pFile, "\n %s\n", buffer);
   	SformatOutput(buffer, MBUFFER-1, "| RF Silent                0x%X      |  Bluetooth               0x%X      |",
        pBaseEepHeader->rfSilent,
        pBaseEepHeader->opCapBrdFlags.blueToothOptions
        );
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1, "| GPIO wlan Disable        NA       |  GPIO wlan LED           0x%02x     |",
        pBaseEepHeader->wlanLedGpio);
  	fprintf(pFile, "\n %s\n", buffer);
    if (pBaseEepHeader->eeprom_version == QC98XX_EEP_VER1)
    {
        SformatOutput(buffer, MBUFFER-1, "| txrxGain                 0x%02x     |  pwrTableOffset          %d        |",
            pBaseEepHeader->txrxgain, pBaseEepHeader->pwrTableOffset);
	  	fprintf(pFile, "\n %s\n", buffer);
    }
    else //v2
    {
        SformatOutput(buffer, MBUFFER-1, "| txrxGain          see Modal Section  |  pwrTableOffset          %d        |",
            pBaseEepHeader->pwrTableOffset);
	  	fprintf(pFile, "\n %s\n", buffer);
    }
    SformatOutput(buffer, MBUFFER-1, "| spurBaseA                %d        |  spurBaseB               %d        |",
        pBaseEepHeader->spurBaseA, pBaseEepHeader->spurBaseB);
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1, "| spurRssiThresh           %d        |  spurRssiThreshCck       %d        |",
        pBaseEepHeader->spurRssiThresh,pBaseEepHeader->spurRssiThreshCck);
  	fprintf(pFile, "\n %s\n", buffer);
    SformatOutput(buffer, MBUFFER-1, "| spurMitFlag            0x%08x |  internal regulator      0x%02x     |",
         pBaseEepHeader->spurMitFlag,pBaseEepHeader->swreg);
  	fprintf(pFile, "\n %s\n", buffer);
	uint32_t ui=rev(pBaseEepHeader->opCapBrdFlags.boardFlags);
    SformatOutput(buffer, MBUFFER-1, "| boardFlags             0x%08x |  featureEnable         0x%08x |",
        ui, pBaseEepHeader->opCapBrdFlags.featureEnable);
  	fprintf(pFile, "\n %s\n", buffer);
}


void PrintQc98xx_2GHzHeader(int client, const BIMODAL_EEP_HEADER *pBiModalHeader, const QC98XX_EEPROM *pEeprom)
{
    char  buffer[MBUFFER];
    int i, j;
    const FREQ_MODAL_EEP_HEADER *pFreqHeader = &pEeprom->freqModalHeader;

    SformatOutput(buffer, MBUFFER-1," |                                                                       |");
	fprintf(pFile, "%s", buffer);

    SformatOutput(buffer, MBUFFER-1," |===========================2GHz Modal Header===========================|");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | Antenna Common        0x%08LX  |  Antenna Common2       0x%08LX |",
        (long long unsigned int)rev(pBiModalHeader->antCtrlCommon),
        (long long unsigned int)rev(pBiModalHeader->antCtrlCommon2)
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | Ant Chain0  0x%04X    |  Ant Chain1  0x%04X   |  Ant Chain2  0x%04X   |",
        pBiModalHeader->antCtrlChain[0],
        pBiModalHeader->antCtrlChain[1],
        pBiModalHeader->antCtrlChain[2]
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | xatten1DB Ch0  0x%02x   | xatten1DB Ch1  0x%02x   | xatten1DB Ch2  0x%02x   |",
        pFreqHeader->xatten1DB[0].value2G,
        pFreqHeader->xatten1DB[1].value2G,
        pFreqHeader->xatten1DB[2].value2G
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | xatten1Margin Ch0 0x%02x| xatten1Margin Ch1 0x%02x| xatten1Margin Ch2 0x%02x|",
        pFreqHeader->xatten1Margin[0].value2G,
        pFreqHeader->xatten1Margin[1].value2G,
        pFreqHeader->xatten1Margin[2].value2G
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | Volt Slope Ch0  %3d   | Volt Slope Ch1  %3d   | Volt Slope Ch2  %3d   |",
        pBiModalHeader->voltSlope[0],
        pBiModalHeader->voltSlope[1],
        pBiModalHeader->voltSlope[2]
        );
	fprintf(pFile, "%s\n", buffer);

	for (i=0; i<WHAL_NUM_CHAINS; i++) {
        SformatOutput(buffer, MBUFFER-1, " | Alpha Thermal Slope CH%d                                               |", i);
		fprintf(pFile, "%s\n", buffer);

        for (j=0; j<QC98XX_NUM_ALPHATHERM_CHANS_2G; j=j+2) {
            SformatOutput(buffer, MBUFFER-1, " | %04d : %02d, %02d, %02d, %02d             |  %04d : %02d, %02d, %02d, %02d            |",
                    WHAL_FBIN2FREQ(alphaThermChans2G[j], 1),
                    pEeprom->tempComp2G.alphaThermTbl[i][j][0],
                    pEeprom->tempComp2G.alphaThermTbl[i][j][1],
                    pEeprom->tempComp2G.alphaThermTbl[i][j][2],
                    pEeprom->tempComp2G.alphaThermTbl[i][j][3],
                    WHAL_FBIN2FREQ(alphaThermChans2G[j+1], 1),
                    pEeprom->tempComp2G.alphaThermTbl[i][j+1][0],
                    pEeprom->tempComp2G.alphaThermTbl[i][j+1][1],
                    pEeprom->tempComp2G.alphaThermTbl[i][j+1][2],
                    pEeprom->tempComp2G.alphaThermTbl[i][j+1][3]
                    );
			fprintf(pFile, "%s\n", buffer);

        }
    }

    for (i=0; i<QC98XX_EEPROM_MODAL_SPURS; i++) {
        SformatOutput(buffer, MBUFFER-1," | spurChan[%d]             0x%02x      |                                   |",
            i, pBiModalHeader->spurChans[i].spurChan
            );
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1," | spurA_PrimSecChoose[%d]  0x%02x      |  spurB_PrimSecChoose[%d] 0x%02x      |",
            i, pBiModalHeader->spurChans[i].spurA_PrimSecChoose, i, pBiModalHeader->spurChans[i].spurB_PrimSecChoose
            );
		fprintf(pFile, "%s\n", buffer);

    }


	 SformatOutput(buffer, MBUFFER-1," | xpaBiasLvl              0x%02x      |  antennaGainCh        %3d         |",
        pBiModalHeader->xpaBiasLvl,
        pBiModalHeader->antennaGainCh
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | rxFilterCap             0x%02x      |  rxGainCap              0x%02x      |",
        pBiModalHeader->rxFilterCap,
        pBiModalHeader->rxGainCap
        );
	fprintf(pFile, "%s\n", buffer);

    if (pEeprom->baseEepHeader.eeprom_version == QC98XX_EEP_VER1)
    {
        SformatOutput(buffer, MBUFFER-1," | txGain                  0x%02x      |  rxGain                 0x%02x      |",
            (pBiModalHeader->txrxgain >> 4) & 0xf,
            pBiModalHeader->rxGainCap & 0xf
            );
		fprintf(pFile, "%s\n", buffer);

    }
    SformatOutput(buffer, MBUFFER-1," |                                   |                                   |");
	fprintf(pFile, "%s\n", buffer);

}


void PrintQc98xx_2GHzCalData(int client, const QC98XX_EEPROM *pEeprom)
{
    A_UINT16 numPiers = WHAL_NUM_11G_CAL_PIERS;
    A_UINT16 pc; //pier count
    char  buffer[MBUFFER];
    A_UINT16 a,b;
    const A_UINT8 *pPiers = &pEeprom->calFreqPier2G[0];
    const CAL_DATA_PER_FREQ_OLPC *pData = &pEeprom->calPierData2G[0];
    int i;

    SformatOutput(buffer, MBUFFER-1, " |=================2G Power Calibration Information =====================|");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    for (i = 0; i < WHAL_NUM_CHAINS; ++i) {
        SformatOutput(buffer, MBUFFER-1, " |                          Chain %d                                      |", i);
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1, " |-----------------------------------------------------------------------|");
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1, " | Freq  txgainIdx0 dacGain0  Pwr0  txgainIdx1 dacGain1 Pwr1  Volt  Temp |");
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1, " |-----------------------------------------------------------------------|");
		fprintf(pFile, "%s\n", buffer);

        for(pc = 0; pc < numPiers; pc++) {
        a = (pData[pc].calPerPoint[i].power_t8[0]);
        b = (pData[pc].calPerPoint[i].power_t8[1]);
        a = (a>>8)|((a&0xff)<<8);
        b = (b>>8)|((b&0xff)<<8);
            SformatOutput(buffer, MBUFFER-1, " | %04d    %4d      %3d     %3d      %4d      %3d     %3d   %3d   %3d  |",
                WHAL_FBIN2FREQ(pPiers[pc], 1),
                pData[pc].calPerPoint[i].txgainIdx[0], pData[pc].dacGain[0], a/8,
                pData[pc].calPerPoint[i].txgainIdx[1], pData[pc].dacGain[1], b/8,
                pData[pc].voltCalVal, pData[pc].thermCalVal
                );
		fprintf(pFile, "%s\n", buffer);

        }
        SformatOutput(buffer, MBUFFER-1, " |-----------------------------------------------------------------------|");
		fprintf(pFile, "%s\n", buffer);

    }
    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

}

void PrintQc98xx_2GLegacyTargetPower(int client, const CAL_TARGET_POWER_LEG *pVals, const A_UINT8 *pFreq)
{
    int i,j;
    char buffer[MBUFFER],buffer2[MBUFFER];

    SformatOutput(buffer, MBUFFER-1, " |===========================2G Target Powers============================|");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | OFDM   ");
	fprintf(pFile, "%s", buffer);


    for (j = 0; j < WHAL_NUM_11G_LEG_TARGET_POWER_CHANS; j++) {
        SformatOutput(buffer2, MBUFFER-1,"|        %04d        ", WHAL_FBIN2FREQ(*(pFreq+j),1));
		fprintf(pFile, "%s", buffer2);

        STRCAT_BUF(buffer, buffer2);
    }

    STRCAT_BUF(buffer, "|");

    SformatOutput(buffer, MBUFFER-1," |========|====================|====================|====================|");
	fprintf(pFile, "\n %s\n", buffer);


    for (j = 0; j < WHAL_NUM_LEGACY_TARGET_POWER_RATES; j++) {
        SformatOutput(buffer, MBUFFER-1," | %s   ",sRatePrintLegacy[j]);
		fprintf(pFile, "%s", buffer);

        for(i=0; i<WHAL_NUM_11G_LEG_TARGET_POWER_CHANS; i++) {
            SformatOutput(buffer2, MBUFFER-1,"|        %2d.%d        ", pVals[i].tPow2x[j]/2, (pVals[i].tPow2x[j] % 2) * 5);
			fprintf(pFile, "%s", buffer2);

            STRCAT_BUF(buffer, buffer2);
        }
        fprintf(pFile, "\n");
        STRCAT_BUF(buffer, "|");
    }
    SformatOutput(buffer, MBUFFER-1," |========================================================================");
	fprintf(pFile, "%s\n", buffer);

}

void PrintQc98xx_2GCCKTargetPower(int client, const CAL_TARGET_POWER_11B *pVals, const A_UINT8 *pFreq)
{
    int i,j;
    char buffer[MBUFFER],buffer2[MBUFFER];

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | CCK    ");
	fprintf(pFile, "%s", buffer);


    for (j = 0; j < WHAL_NUM_11B_TARGET_POWER_CHANS; j++) {
        SformatOutput(buffer2, MBUFFER-1,"|        %04d        ", WHAL_FBIN2FREQ(*(pFreq+j),1));
		fprintf(pFile, "%s", buffer2);

        STRCAT_BUF(buffer, buffer2);
    }

    STRCAT_BUF(buffer, "|====================|");

    SformatOutput(buffer, MBUFFER-1," |========|====================|====================|====================|");
	fprintf(pFile, "\n %s\n", buffer);


    for (j = 0; j < WHAL_NUM_11B_TARGET_POWER_RATES; j++) {
        SformatOutput(buffer, MBUFFER-1," | %s  ",sRatePrintCck[j]);
		fprintf(pFile, "%s", buffer);

        for(i=0; i<WHAL_NUM_11B_TARGET_POWER_CHANS; i++) {
            SformatOutput(buffer2, MBUFFER-1,"|        %2d.%d        ", pVals[i].tPow2x[j]/2, (pVals[i].tPow2x[j] % 2) * 5);
			fprintf(pFile, "%s", buffer2);

            STRCAT_BUF(buffer, buffer2);
        }

        STRCAT_BUF(buffer, "|====================|");
        fprintf(pFile, "\n");
    }
    SformatOutput(buffer, MBUFFER-1," |========================================================================");
	fprintf(pFile, "%s\n", buffer);

}

void PrintQc98xx_2GHT20TargetPower(int client, const CAL_TARGET_POWER_11G_20 *pVals, const A_UINT8 *pFreq)
{
    int i,j;
    char buffer[MBUFFER],buffer2[MBUFFER];
    double power;
    int rateIndex;

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | VHT20     ");
	fprintf(pFile, "%s", buffer);


    for (j = 0; j < WHAL_NUM_11G_20_TARGET_POWER_CHANS; j++) {
        SformatOutput(buffer2, MBUFFER-1,"|        %04d       ", WHAL_FBIN2FREQ(*(pFreq+j),1));
		fprintf(pFile, "%s", buffer2);

        STRCAT_BUF(buffer, buffer2);
    }

    STRCAT_BUF(buffer, "|");

    SformatOutput(buffer, MBUFFER-1," |===========|===================|===================|===================|");
	fprintf(pFile, "\n %s\n", buffer);

    for (j = 0; j < RATE_PRINT_VHT_SIZE; j++) {
        SformatOutput(buffer, MBUFFER-1," | %s  ",sRatePrintVHT[j]);
		fprintf(pFile, "%s", buffer);

        for(i=0; i<WHAL_NUM_11G_20_TARGET_POWER_CHANS; i++) {
            rateIndex = vRATE_INDEX_HT20_MCS0 + j;
            Qc98xxTargetPowerGet(WHAL_FBIN2FREQ(*(pFreq+i),1), rateIndex, &power);
            SformatOutput(buffer2, MBUFFER-1,"|        %2.1f       ", power);
			fprintf(pFile, "%s", buffer2);

            STRCAT_BUF(buffer, buffer2);
        }

        STRCAT_BUF(buffer, "|");
        fprintf(pFile, "\n");
    }
    SformatOutput(buffer, MBUFFER-1," |========================================================================");
	fprintf(pFile, "%s\n", buffer);

}


void PrintQc98xx_2GHT40TargetPower(int client, const CAL_TARGET_POWER_11G_40 *pVals, const A_UINT8 *pFreq)
{
    int i,j;
    char buffer[MBUFFER],buffer2[MBUFFER];
    double power;
    int rateIndex;

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | VHT40     ");
	fprintf(pFile, "%s", buffer);


    for (j = 0; j < WHAL_NUM_11G_40_TARGET_POWER_CHANS; j++) {
        SformatOutput(buffer2, MBUFFER-1,"|        %04d       ", WHAL_FBIN2FREQ(*(pFreq+j),1));
		fprintf(pFile, "%s", buffer2);

        STRCAT_BUF(buffer, buffer2);
    }

    STRCAT_BUF(buffer, "|");

    SformatOutput(buffer, MBUFFER-1," |===========|===================|===================|===================|");
	fprintf(pFile, "\n %s\n", buffer);


    for (j = 0; j < RATE_PRINT_VHT_SIZE; j++) {
        SformatOutput(buffer, MBUFFER-1," | %s  ",sRatePrintVHT[j]);
		fprintf(pFile, "%s", buffer);

        for(i=0; i < WHAL_NUM_11G_40_TARGET_POWER_CHANS; i++) {
            rateIndex = vRATE_INDEX_HT40_MCS0 + j;
            Qc98xxTargetPowerGet(WHAL_FBIN2FREQ(*(pFreq+i),1), rateIndex, &power);
            SformatOutput(buffer2, MBUFFER-1,"|        %2.1f       ", power);
			fprintf(pFile, "%s", buffer2);

            STRCAT_BUF(buffer, buffer2);
        }

        STRCAT_BUF(buffer, "|");
        fprintf(pFile, "\n");
    }
    SformatOutput(buffer, MBUFFER-1," |========================================================================");
	fprintf(pFile, "%s\n", buffer);

}



void PrintQc98xx_2GCTLIndex(int client, const A_UINT8 *pCtlIndex){
    int i;
    char buffer[MBUFFER];
    SformatOutput(buffer, MBUFFER-1, "2G CTL Index:");
	fprintf(pFile, "%s", buffer);

    for(i=0; i < WHAL_NUM_CTLS_2G; i++) {
        if (pCtlIndex[i] == 0)
        {
            continue;
        }
        SformatOutput(buffer, MBUFFER-1, "[%d] :0x%x", i, pCtlIndex[i]);
		fprintf(pFile, "%s", buffer);

    }
}

void PrintQc98xx_2GCTLData(int client,  const A_UINT8 *ctlIndex, const CAL_CTL_DATA_2G Data[WHAL_NUM_CTLS_2G], const A_UINT8 *pFreq)
{

    int i,j;
    char buffer[MBUFFER], buffer2[MBUFFER];

    SformatOutput(buffer, MBUFFER-1," |                                                                       |");
	fprintf(pFile, "%s\n", buffer);


    SformatOutput(buffer, MBUFFER-1, " |=======================Test Group Band Edge Power======================|");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    for (i = 0; i < WHAL_NUM_CTLS_2G; i++)
    {
        if (ctlIndex[i] == 0)
        {
            pFreq+=WHAL_NUM_BAND_EDGES_2G;
            continue;
        }
        SformatOutput(buffer, MBUFFER-1," |                                                                       |");
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1," | CTL: 0x%02x %s                                           |",
                 ctlIndex[i], sCtlType[ctlIndex[i] & QC98XX_CTL_MODE_M]);
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1," |=======|=======|=======|=======|=======|=======|=======|=======|=======|");
		fprintf(pFile, "%s\n", buffer);

        //WHAL_FBIN2FREQ(*(pFreq++),0),Data[i].ctlEdges[j].tPower,Data[i].ctlEdges[j].flag

		SformatOutput(buffer, MBUFFER-1," | edge  ");
		fprintf(pFile, "%s", buffer);

        for (j = 0; j < WHAL_NUM_BAND_EDGES_2G; j++) {
        	if (*(pFreq+j) == QC98XX_BCHAN_UNUSED) {
            	SformatOutput(buffer2, MBUFFER-1,"|  --   ");fprintf(pFile,"%s",buffer2);
                } else {
                	SformatOutput(buffer2, MBUFFER-1,"| %04d  ", WHAL_FBIN2FREQ(*(pFreq+j),1));
                    fprintf(pFile, "%s", buffer2);
                }
                STRCAT_BUF(buffer, buffer2);
            }

		STRCAT_BUF(buffer, "|");
        fprintf(pFile, "\n");

        SformatOutput(buffer, MBUFFER-1," |=======|=======|=======|=======|=======|=======|=======|=======|=======|");
		fprintf(pFile, "%s\n", buffer);

		SformatOutput(buffer, MBUFFER-1," | power ");
		fprintf(pFile, "%s", buffer);


        for (j = 0; j < WHAL_NUM_BAND_EDGES_2G; j++) {
        	if (*(pFreq+j) == QC98XX_BCHAN_UNUSED) {
            	SformatOutput(buffer2, MBUFFER-1,"|  --   ");
				fprintf(pFile, "%s", buffer2);
				} else {
                    SformatOutput(buffer2, MBUFFER-1,"| %2d.%d  ", Data[i].ctl_edges[j].u.tPower / 2,
                        (Data[i].ctl_edges[j].u.tPower % 2) * 5);
						fprintf(pFile, "%s", buffer2);

                }
                STRCAT_BUF(buffer, buffer2);
            }

		STRCAT_BUF(buffer, "|");

        SformatOutput(buffer, MBUFFER-1," |=======|=======|=======|=======|=======|=======|=======|=======|=======|");
		fprintf(pFile, "\n%s\n", buffer);


        SformatOutput(buffer, MBUFFER-1, " | flag  ");
		fprintf(pFile, "%s", buffer);


         for (j = 0; j < WHAL_NUM_BAND_EDGES_2G; j++) {
         	if (*(pFreq+j) == QC98XX_BCHAN_UNUSED) {
            	SformatOutput(buffer2, MBUFFER-1, "|  --   ");
				fprintf(pFile, "%s", buffer2);
				} else {
                    SformatOutput(buffer2, MBUFFER-1,"|   %1d   ", Data[i].ctl_edges[j].u.flag);
					fprintf(pFile, "%s", buffer2);

                }
                STRCAT_BUF(buffer, buffer2);
            }

		STRCAT_BUF(buffer, "|");

        pFreq+=WHAL_NUM_BAND_EDGES_2G;

        SformatOutput(buffer, MBUFFER-1," =========================================================================");
		fprintf(pFile, "\n%s\n", buffer);

    }
}

void PrintQc98xx_5GHzHeader(int client, const BIMODAL_EEP_HEADER * pBiModalHeader, const QC98XX_EEPROM *pEeprom)
{
    char  buffer[MBUFFER];
    int i, j;
    const FREQ_MODAL_EEP_HEADER *pFreqHeader = &pEeprom->freqModalHeader;
    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," |===========================5GHz Modal Header===========================|");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," |                                                                       |");
	fprintf(pFile, "%s\n", buffer);


	SformatOutput(buffer, MBUFFER-1," | Antenna Common        0x%08LX  |  Antenna Common2       0x%08LX |",
	    (long long unsigned int) rev(pBiModalHeader->antCtrlCommon),
	    (long long unsigned int) rev(pBiModalHeader->antCtrlCommon2)
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | Ant Chain0  0x%04X   |  Ant Chain1  0x%04X   |  Ant Chain2  0x%04X    |",
        rev2(pBiModalHeader->antCtrlChain[0]),
        rev2(pBiModalHeader->antCtrlChain[1]),
        rev2(pBiModalHeader->antCtrlChain[2])
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | xatten1DB Ch0  (0x%02x,0x%02x,0x%02x)   | xatten1DB Ch1  (0x%02x,0x%02x,0x%02x)   |",
        pFreqHeader->xatten1DB[0].value5GLow, pFreqHeader->xatten1DB[0].value5GMid, pFreqHeader->xatten1DB[0].value5GHigh,
        pFreqHeader->xatten1DB[1].value5GLow, pFreqHeader->xatten1DB[1].value5GMid, pFreqHeader->xatten1DB[1].value5GHigh
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | xatten1DB Ch2  (0x%02x,0x%02x,0x%02x)   | xatten1Margin Ch0 (0x%02x,0x%02x,0x%02x)|",
        pFreqHeader->xatten1DB[2].value5GLow, pFreqHeader->xatten1DB[2].value5GMid, pFreqHeader->xatten1DB[2].value5GHigh,
        pFreqHeader->xatten1Margin[0].value5GLow, pFreqHeader->xatten1Margin[0].value5GMid, pFreqHeader->xatten1Margin[0].value5GHigh
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | xatten1Margin Ch1 (0x%02x,0x%02x,0x%02x)| xatten1Margin Ch2 (0x%02x,0x%02x,0x%02x)|",
        pFreqHeader->xatten1Margin[1].value5GLow, pFreqHeader->xatten1Margin[1].value5GMid, pFreqHeader->xatten1Margin[1].value5GHigh,
        pFreqHeader->xatten1Margin[2].value5GLow, pFreqHeader->xatten1Margin[2].value5GMid, pFreqHeader->xatten1Margin[2].value5GHigh
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | Volt Slope Ch0  %3d  | Volt Slope Ch1  %3d   | Volt Slope Ch2  %3d    |",
        pBiModalHeader->voltSlope[0],
        pBiModalHeader->voltSlope[1],
        pBiModalHeader->voltSlope[2]
        );
	fprintf(pFile, "%s\n", buffer);

	for (i=0; i < WHAL_NUM_CHAINS; i++) {
        SformatOutput(buffer, MBUFFER-1, " | Alpha Thermal Slope CH%d                                               |", i);
		fprintf(pFile, "%s\n", buffer);

        for (j=0; j < QC98XX_NUM_ALPHATHERM_CHANS_2G; j=j+2)
        {
            SformatOutput(buffer, MBUFFER-1, " | %04d : %02d, %02d, %02d, %02d             |  %04d : %02d, %02d, %02d, %02d            |",
                    WHAL_FBIN2FREQ(alphaThermChans5G[j], 0),
                    pEeprom->tempComp5G.alphaThermTbl[i][j][0],
                    pEeprom->tempComp5G.alphaThermTbl[i][j][1],
                    pEeprom->tempComp5G.alphaThermTbl[i][j][2],
                    pEeprom->tempComp5G.alphaThermTbl[i][j][3],
                    WHAL_FBIN2FREQ(alphaThermChans5G[j+1], 0),
                    pEeprom->tempComp5G.alphaThermTbl[i][j+1][0],
                    pEeprom->tempComp5G.alphaThermTbl[i][j+1][1],
                    pEeprom->tempComp5G.alphaThermTbl[i][j+1][2],
                    pEeprom->tempComp5G.alphaThermTbl[i][j+1][3]
                    );
			fprintf(pFile, "%s\n", buffer);

        }
    }
    for (i=0; i < QC98XX_EEPROM_MODAL_SPURS; i++)
    {
        SformatOutput(buffer, MBUFFER-1," | spurChan[%d]             0x%02x      |                                   |",
            i, pBiModalHeader->spurChans[i].spurChan
            );
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1," | spurA_PrimSecChoose[%d]  0x%02x      |  spurA_PrimSecChoose[%d] 0x%02x      |",
            i, pBiModalHeader->spurChans[i].spurA_PrimSecChoose, i, pBiModalHeader->spurChans[i].spurB_PrimSecChoose
            );
		fprintf(pFile, "%s\n", buffer);

    }

	SformatOutput(buffer, MBUFFER-1," | xpaBiasLvl              0x%02x      |  antennaGainCh         %3d        |",
        pBiModalHeader->xpaBiasLvl,
        pBiModalHeader->antennaGainCh
        );
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," | rxFilterCap             0x%02x      |  rxGainCap              0x%02x      |",
        pBiModalHeader->rxFilterCap,
        pBiModalHeader->rxGainCap
        );
	fprintf(pFile, "%s\n", buffer);

    if (pEeprom->baseEepHeader.eeprom_version == QC98XX_EEP_VER1)
    {
        SformatOutput(buffer, MBUFFER-1," | txGain                  0x%02x      |  rxGain                 0x%02x      |",
            (pBiModalHeader->txrxgain >> 4) & 0xf,
            pBiModalHeader->rxGainCap & 0xf
            );
		fprintf(pFile, "%s\n", buffer);

    }

    SformatOutput(buffer, MBUFFER-1," |                                   |                                   |");
	fprintf(pFile, "%s\n", buffer);

}

void PrintQc98xx_5GHzCalData(int client, const QC98XX_EEPROM *pEeprom)
{
    A_UINT16 numPiers = WHAL_NUM_11A_CAL_PIERS;
    A_UINT16 pc; //pier count
	A_UINT16 a,b;
    char  buffer[MBUFFER];
    const A_UINT8 *pPiers = &pEeprom->calFreqPier5G[0];
    const CAL_DATA_PER_FREQ_OLPC *pData = &pEeprom->calPierData5G[0];
    int i;

    SformatOutput(buffer, MBUFFER-1, " |=================5G Power Calibration Information =====================|");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    for (i = 0; i < WHAL_NUM_CHAINS; ++i)
    {
        SformatOutput(buffer, MBUFFER-1, " |                          Chain %d                                      |", i);
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1, " |-----------------------------------------------------------------------|");
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1, " | Freq  txgainIdx0 dacGain0  Pwr0  txgainIdx1 dacGain1 Pwr1  Volt  Temp |");
		fprintf(pFile, "%s\n", buffer);

        SformatOutput(buffer, MBUFFER-1, " |-----------------------------------------------------------------------|");
		fprintf(pFile, "%s\n", buffer);

        for(pc = 0; pc < numPiers; pc++) {
	   	   	a = (pData[pc].calPerPoint[i].power_t8[0]);
		   	b = (pData[pc].calPerPoint[i].power_t8[1]);
	    	a = (a>>8)|((a&0xff)<<8);
			b = (b>>8)|((b&0xff)<<8);

			SformatOutput(buffer, MBUFFER-1, " | %04d    %4d      %3d     %3d      %4d      %3d     %3d   %3d   %3d  |",
                WHAL_FBIN2FREQ(pPiers[pc], 0),
                pData[pc].calPerPoint[i].txgainIdx[0], pData[pc].dacGain[0], a/8,
                pData[pc].calPerPoint[i].txgainIdx[1], pData[pc].dacGain[1], b/8,
                pData[pc].voltCalVal, pData[pc].thermCalVal
                );
			fprintf(pFile, "%s\n", buffer);

        }
        SformatOutput(buffer, MBUFFER-1, " |-----------------------------------------------------------------------|");
		fprintf(pFile, "%s\n", buffer);

    }
    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

}



void PrintQc98xx_5GLegacyTargetPower(int client, const CAL_TARGET_POWER_LEG *pVals, const A_UINT8 *pFreq)
{
    int i,j;
    char buffer[MBUFFER],buffer2[MBUFFER];

    SformatOutput(buffer, MBUFFER-1, " |===========================5G Target Powers============================|");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," |  OFDM     ");
	fprintf(pFile, "%s", buffer);


    for (j = 0; j < WHAL_NUM_11A_LEG_TARGET_POWER_CHANS; j++) {
        SformatOutput(buffer2, MBUFFER-1, "|  %04d   ", WHAL_FBIN2FREQ(*(pFreq+j),0));
		fprintf(pFile, "%s", buffer2);

        STRCAT_BUF(buffer, buffer2);
    }

    STRCAT_BUF(buffer, "|");

    SformatOutput(buffer, MBUFFER-1,"|===========|=========|=========|=========|=========|=========|=========|");
	fprintf(pFile, "\n %s\n", buffer);


    for (j = 0; j < WHAL_NUM_LEGACY_TARGET_POWER_RATES; j++) {
        SformatOutput(buffer, MBUFFER-1," |  %s     ",sRatePrintLegacy[j]);
		fprintf(pFile, "%s", buffer);

        for(i=0; i < WHAL_NUM_11A_LEG_TARGET_POWER_CHANS; i++) {
            SformatOutput(buffer2, MBUFFER-1,"|  %2d.%d   ", pVals[i].tPow2x[j]/2, (pVals[i].tPow2x[j] % 2) * 5);
			fprintf(pFile, "%s", buffer2);

            STRCAT_BUF(buffer, buffer2);
        }
	fprintf(pFile, "\n");
    STRCAT_BUF(buffer, "|");
    }
    SformatOutput(buffer, MBUFFER-1," |========================================================================");
	fprintf(pFile, "%s\n", buffer);

}

void PrintQc98xx_5GHT20TargetPower(int client, const CAL_TARGET_POWER_11A_20 *pVals, const A_UINT8 *pFreq)
{
    int i,j;
    char buffer[MBUFFER],buffer2[MBUFFER];
    double power;
    int rateIndex;

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," |  VHT20    ");
	fprintf(pFile, "%s", buffer);


    for (j = 0; j < WHAL_NUM_11A_20_TARGET_POWER_CHANS; j++)
    {
        SformatOutput(buffer2, MBUFFER-1,"|  %04d   ", WHAL_FBIN2FREQ(*(pFreq+j),0));
		fprintf(pFile, "%s", buffer2);

        STRCAT_BUF(buffer, buffer2);
    }

    STRCAT_BUF(buffer, "|");

    SformatOutput(buffer, MBUFFER-1,"|===========|=========|=========|=========|=========|=========|=========|");
	fprintf(pFile, "\n %s\n", buffer);


    for (j = 0; j < RATE_PRINT_VHT_SIZE; j++) {
        SformatOutput(buffer, MBUFFER-1," |  %s ",sRatePrintVHT[j]);
		fprintf(pFile, "%s", buffer);

        for(i=0; i < WHAL_NUM_11A_20_TARGET_POWER_CHANS; i++) {
            rateIndex = vRATE_INDEX_HT20_MCS0 + j;
            Qc98xxTargetPowerGet(WHAL_FBIN2FREQ(*(pFreq+i),0), rateIndex, &power);
            SformatOutput(buffer2, MBUFFER-1,"|  %2.1f   ", power);
			fprintf(pFile, "%s", buffer2);

            STRCAT_BUF(buffer, buffer2);
        }
    STRCAT_BUF(buffer, "|");
	fprintf(pFile, "\n");
    }
    SformatOutput(buffer, MBUFFER-1," |========================================================================");
	fprintf(pFile, "%s\n", buffer);

}



void PrintQc98xx_5GHT40TargetPower(int client, const CAL_TARGET_POWER_11A_40 *pVals, const A_UINT8 *pFreq)
{
    int i,j;
    char buffer[MBUFFER], buffer2[MBUFFER];
    double power;
    int rateIndex;

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," |  VHT40    ");
	fprintf(pFile, "%s", buffer);


    for (j = 0; j < WHAL_NUM_11A_40_TARGET_POWER_CHANS; j++) {
        SformatOutput(buffer2, MBUFFER-1,"|  %04d   ", WHAL_FBIN2FREQ(*(pFreq+j),0));
		fprintf(pFile, "%s", buffer2);

        STRCAT_BUF(buffer, buffer2);
    }

    STRCAT_BUF(buffer, "|");

    SformatOutput(buffer, MBUFFER-1,"|===========|=========|=========|=========|=========|=========|=========|");
	fprintf(pFile, "\n %s\n", buffer);


    for (j = 0; j < RATE_PRINT_VHT_SIZE; j++) {
        SformatOutput(buffer, MBUFFER-1," |  %s ", sRatePrintVHT[j]);
		fprintf(pFile, "%s", buffer);

        for(i=0; i < WHAL_NUM_11A_40_TARGET_POWER_CHANS; i++) {
            rateIndex = vRATE_INDEX_HT40_MCS0 + j;
            Qc98xxTargetPowerGet(WHAL_FBIN2FREQ(*(pFreq+i),0), rateIndex, &power);
            SformatOutput(buffer2, MBUFFER-1,"|  %2.1f   ", power);
			fprintf(pFile, "%s", buffer2);

            STRCAT_BUF(buffer, buffer2);
        }
    STRCAT_BUF(buffer, "|");
    fprintf(pFile, "\n");
	}
    SformatOutput(buffer, MBUFFER-1," |========================================================================");
	fprintf(pFile, "%s\n", buffer);

}


void PrintQc98xx_5GHT80TargetPower(int client, const CAL_TARGET_POWER_11A_80 *pVals, const A_UINT8 *pFreq)
{
    int i,j;
    char buffer[MBUFFER], buffer2[MBUFFER];
    double power;
    int rateIndex;
    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," |  VHT80    ");
	fprintf(pFile, "%s", buffer);


    for (j = 0; j < WHAL_NUM_11A_80_TARGET_POWER_CHANS; j++) {
        SformatOutput(buffer2, MBUFFER-1,"|  %04d   ", WHAL_FBIN2FREQ(*(pFreq+j),0));
		fprintf(pFile, "%s", buffer2);

        STRCAT_BUF(buffer, buffer2);
    }

    STRCAT_BUF(buffer, "|");

    SformatOutput(buffer, MBUFFER-1,"|===========|=========|=========|=========|=========|=========|=========|");
	fprintf(pFile, "\n %s\n", buffer);


    for (j = 0; j < RATE_PRINT_VHT_SIZE; j++) {
        SformatOutput(buffer, MBUFFER-1," |  %s ", sRatePrintVHT[j]);
		fprintf(pFile, "%s", buffer);

        for(i=0; i < WHAL_NUM_11A_80_TARGET_POWER_CHANS; i++) {
            rateIndex = vRATE_INDEX_HT80_MCS0 + j;
            Qc98xxTargetPowerGet(WHAL_FBIN2FREQ(*(pFreq+i),0), rateIndex, &power);
            SformatOutput(buffer2, MBUFFER-1,"|  %2.1f   ", power);
			fprintf(pFile, "%s", buffer2);

            STRCAT_BUF(buffer, buffer2);
        }
        STRCAT_BUF(buffer, "|");
		fprintf(pFile, "\n");
    }
    SformatOutput(buffer, MBUFFER-1," |========================================================================");
	fprintf(pFile, "%s\n", buffer);
}



void PrintQc98xx_5GCTLIndex(int client, const A_UINT8 *pCtlIndex){
    int i;
    char buffer[MBUFFER];
    SformatOutput(buffer, MBUFFER-1, "5G CTL Index:");
    for(i=0; i < WHAL_NUM_CTLS_5G; i++) {
        if (pCtlIndex[i] == 0)
        {
            continue;
        }
        SformatOutput(buffer, MBUFFER-1,"[%d] :0x%x", i, pCtlIndex[i]);
    }
}



void PrintQc98xx_5GCTLData(int client, const A_UINT8 *ctlIndex, const CAL_CTL_DATA_5G Data[WHAL_NUM_CTLS_5G], const A_UINT8 *pFreq)
{

    int i,j;
    char buffer[MBUFFER],buffer2[MBUFFER];

    SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
	fprintf(pFile, "%s\n", buffer);


    SformatOutput(buffer, MBUFFER-1," |=======================Test Group Band Edge Power======================|");
	fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," |                                                                       |");
	fprintf(pFile, "%s\n", buffer);

    for (i = 0; i < WHAL_NUM_CTLS_5G; i++)
    {
        if (ctlIndex[i] == 0)
        {
            pFreq+=WHAL_NUM_BAND_EDGES_5G;
            continue;
        }
        SformatOutput(buffer, MBUFFER-1, " |                                                                       |");
		fprintf(pFile, "%s\n", buffer);
        SformatOutput(buffer, MBUFFER-1," |=======|=======|=======|=======|=======|=======|=======|=======|=======|");
		fprintf(pFile, "%s\n", buffer);
		SformatOutput(buffer, MBUFFER-1, " | edge  ");
		fprintf(pFile, "%s", buffer);

        for (j = 0; j < WHAL_NUM_BAND_EDGES_5G; j++) {
            if (*(pFreq+j) == QC98XX_BCHAN_UNUSED) {
                SformatOutput(buffer2, MBUFFER-1,"|  --   ");
				fprintf(pFile, "%s", buffer2);

            } else {
                SformatOutput(buffer2, MBUFFER-1,"| %04d  ", WHAL_FBIN2FREQ(*(pFreq+j),0));
				fprintf(pFile, "%s", buffer2);

            }
            STRCAT_BUF(buffer, buffer2);
        }

	STRCAT_BUF(buffer, "|");
	fprintf(pFile, "\n");

    SformatOutput(buffer, MBUFFER-1," |=======|=======|=======|=======|=======|=======|=======|=======|=======|");
	fprintf(pFile, "%s\n", buffer);


    SformatOutput(buffer, MBUFFER-1," | power ");
	fprintf(pFile, "%s", buffer);


    for (j = 0; j < WHAL_NUM_BAND_EDGES_5G; j++) {
    	if (*(pFreq+j) == QC98XX_BCHAN_UNUSED) {
        	SformatOutput(buffer2, MBUFFER-1,"|  --   ");
			fprintf(pFile, "%s", buffer2);

            } else {
                SformatOutput(buffer2, MBUFFER-1,"| %2d.%d  ", Data[i].ctl_edges[j].u.tPower / 2,
                   (Data[i].ctl_edges[j].u.tPower % 2) * 5);
				fprintf(pFile, "%s", buffer2);

            }
            STRCAT_BUF(buffer, buffer2);
        }

        STRCAT_BUF(buffer, "|");
        SformatOutput(buffer, MBUFFER-1," |=======|=======|=======|=======|=======|=======|=======|=======|=======|");
		fprintf(pFile, "\n%s\n", buffer);


        SformatOutput(buffer, MBUFFER-1," | flag  ");
		fprintf(pFile, "%s", buffer);


        for (j = 0; j < WHAL_NUM_BAND_EDGES_5G; j++) {
            if (*(pFreq+j) == QC98XX_BCHAN_UNUSED) {
                SformatOutput(buffer2, MBUFFER-1,"|  --   ");
				fprintf(pFile, "%s", buffer2);

            } else {
                SformatOutput(buffer2, MBUFFER-1,"|   %1d   ", Data[i].ctl_edges[j].u.flag);
				fprintf(pFile, "%s", buffer2);

            }
            STRCAT_BUF(buffer, buffer2);
        }

        STRCAT_BUF(buffer, "|");

        pFreq+=WHAL_NUM_BAND_EDGES_5G;

        SformatOutput(buffer, MBUFFER-1," =========================================================================");
		fprintf(pFile, "\n%s\n", buffer);

    }
}

void PrintQc98xxStruct(QC98XX_EEPROM *pEeprom, int client)

{
#define MBUFFER 1024
    char buffer[MBUFFER],buffer2[MBUFFER];
    int i;

    SformatOutput(buffer, MBUFFER-1,"start get all");
	fprintf(pFile, "%s\n", buffer);
    SformatOutput(buffer, MBUFFER-1,"                         ----------------------                           ");
	fprintf(pFile, "%s\n", buffer);
    SformatOutput(buffer, MBUFFER-1,  " =======================| QC98XX CAL STRUCTURE |==========================");
	fprintf(pFile, "%s\n", buffer);
    SformatOutput(buffer, MBUFFER-1," |                       -----------------------                         |");
	fprintf(pFile, "%s\n", buffer);
    SformatOutput(buffer, MBUFFER-1," |                                   |                                   |");
	fprintf(pFile, "%s\n", buffer);
    SformatOutput(buffer, MBUFFER-1," | Board Data Version      %2d        |  Template Version       %2d        |",
        pEeprom->baseEepHeader.eeprom_version,
        pEeprom->baseEepHeader.template_version
        );

    fprintf(pFile, "%s\n", buffer);

    SformatOutput(buffer, MBUFFER-1," |-----------------------------------------------------------------------|");
	fprintf(pFile, "%s\n", buffer);
    SformatOutput(buffer, MBUFFER-1," | MacAddress: 0x%02X:%02X:%02X:%02X:%02X:%02X                                       |",
          pEeprom->baseEepHeader.macAddr[0],pEeprom->baseEepHeader.macAddr[1],pEeprom->baseEepHeader.macAddr[2],
          pEeprom->baseEepHeader.macAddr[3],pEeprom->baseEepHeader.macAddr[4],pEeprom->baseEepHeader.macAddr[5]);
	fprintf(pFile, "%s\n", buffer);
    SformatOutput(buffer, MBUFFER-1," | Customer Data in hex                                                  |");
	fprintf(pFile, "%s\n", buffer);
    SformatOutput(buffer, MBUFFER-1, " |");
	fprintf(pFile, "%s", buffer);
    for(i=0; i < CUSTOMER_DATA_SIZE; i++) {
        SformatOutput(buffer2, MBUFFER-1," %02X", pEeprom->baseEepHeader.custData[i] );
        STRCAT_BUF(buffer, buffer2);
    }
    STRCAT_BUF(buffer, "         |");
	fprintf(pFile, "%s\n", buffer);
    SformatOutput(buffer, MBUFFER-1," |-----------------------------------------------------------------------|");
	fprintf(pFile, "%s", buffer);
	PrintQc98xxBaseHeader(0, &(pEeprom->baseEepHeader), pEeprom);
	if (pEeprom->baseEepHeader.opCapBrdFlags.opFlags & WHAL_OPFLAGS_11G)
    {
        PrintQc98xx_2GHzHeader(client, &(pEeprom->biModalHeader[1]), pEeprom);
        PrintQc98xx_2GHzCalData(client, pEeprom);
        PrintQc98xx_2GLegacyTargetPower(client, pEeprom->targetPower2G, pEeprom->targetFreqbin2G);
        PrintQc98xx_2GCCKTargetPower(client, pEeprom->targetPowerCck, pEeprom->targetFreqbinCck);
        PrintQc98xx_2GHT20TargetPower(client, pEeprom->targetPower2GVHT20, pEeprom->targetFreqbin2GVHT20);
        PrintQc98xx_2GHT40TargetPower(client, pEeprom->targetPower2GVHT40, pEeprom->targetFreqbin2GVHT40);
        PrintQc98xx_2GCTLData(client, pEeprom->ctlIndex2G, pEeprom->ctlData2G, &pEeprom->ctlFreqbin2G[0][0]);
    }
    if (pEeprom->baseEepHeader.opCapBrdFlags.opFlags & WHAL_OPFLAGS_11A)
    {
        PrintQc98xx_5GHzHeader(client, &(pEeprom->biModalHeader[0]), pEeprom);
        PrintQc98xx_5GHzCalData(client, pEeprom);
        PrintQc98xx_5GLegacyTargetPower(client, pEeprom->targetPower5G, pEeprom->targetFreqbin5G);
        PrintQc98xx_5GHT20TargetPower(client, pEeprom->targetPower5GVHT20, pEeprom->targetFreqbin5GVHT20);
        PrintQc98xx_5GHT40TargetPower(client, pEeprom->targetPower5GVHT40, pEeprom->targetFreqbin5GVHT40);
        PrintQc98xx_5GHT80TargetPower(client, pEeprom->targetPower5GVHT80, pEeprom->targetFreqbin5GVHT80);
        PrintQc98xx_5GCTLData(client, pEeprom->ctlIndex5G, pEeprom->ctlData5G, &pEeprom->ctlFreqbin5G[0][0]);
    }

}
/*
 * This is NOT the ideal way to write OTP since it does not handle
 * media errors.  It's much better to use the otpstream_* API.
 * This capability is here to help salvage parts that have previously
 * had OTP written.
 */
void
WriteTargetOTP(int dev, A_UINT32 offset, A_UINT8 *buffer, A_UINT32 length)
{
    A_UINT32 status_mask;
    A_UINT32 otp_status;
    int i;

    /* Enable OTP read/write power */
    WriteTargetWord(dev, REG_RTCSOC_OTP, OTP_VDD12_EN_SET(1) | OTP_LDO25_EN_SET(1));
    status_mask = OTP_STATUS_VDD12_EN_READY_SET(1) | OTP_STATUS_LDO25_EN_READY_SET(1);
    do {
        ReadTargetWord(dev, REG_RTCSOC_OTP_STATUS, &otp_status);
    } while ((otp_status & (OTP_STATUS_VDD12_EN_READY_MASK | OTP_STATUS_LDO25_EN_READY_MASK)) != status_mask);

    /* Conservatively set OTP read/write timing for highest core clock */
    InitializeOtpTimingParameters(dev);

    /* Enable eFuse for write */
    WriteTargetWord(dev, REG_EFUSE_WR_ENABLE, EFUSE_WR_ENABLE_REG_V_SET(1));
    WriteTargetWord(dev, REG_EFUSE_BITMASK_WR, 0x00);

    /* Write data to OTP */
    for (i=0; i<length; i++, offset++) {
        A_UINT32 efuse_word;
        A_UINT32 readback;
        int attempt;

#define EFUSE_WRITE_COUNT 3
        efuse_word = (A_UINT32)buffer[i];
        for (attempt=1; attempt<=EFUSE_WRITE_COUNT; attempt++) {
            WriteTargetWord(dev, REG_EFUSE_INTF0 + (offset<<2), efuse_word);
        }

        /* verify */
        ReadTargetWord(dev, REG_EFUSE_INTF0 + (offset<<2), &readback);
        if (efuse_word != readback) {
            fprintf(stderr, "OTP write failed. Offset=%u, Value=0x%x, Readback=0x%x\n",
                        offset, efuse_word, readback);
            break;
        }
    }

    /* Disable OTP */
    WriteTargetWord(dev, REG_RTCSOC_OTP, 0);
}

unsigned int
parse_address(char *optarg)
{
    unsigned int address;

    /* may want to add support for symbolic addresses here */

    address = strtoul(optarg, NULL, 0);

    return address;
}

void
print_cnt(int dev, unsigned int address) {

    A_UINT32 param;

    nqprintf("DIAG Read Word (address: 0x%x)\n", address);
    ReadTargetWord(dev, address, &param);
    if (quiet()) {
        printf("0x%08x\t", param);
    } else {
        printf("Value in target at 0x%x: 0x%x (%d)\n", address, param, param);
    }
}
int
main (int argc, char **argv) {
    int c, fd, dev;
    FILE * dump_fd = NULL;
    unsigned int address = 0;
    unsigned int length = 0;
    unsigned int wordcount = 1;
    A_UINT32 param = 0;
    char filename[PATH_MAX];
    char devicename[PATH_MAX];
    unsigned int cmd = 0;
    A_UINT8 *buffer = NULL;
    int i, CAL_FLAG = 0;
    unsigned int bitwise_mask = 0;
    A_UINT32 *bufferWord = NULL;
    A_UINT32 *bufferWordLSB = NULL;
    A_UINT32 *bufferWordMSB = NULL;
    A_UINT32 bufferReadAddr = 0;

/* select signals for events to capture...the index is important. By default capture is disabled for all events */
    int StopEventIdx = 0;
    int eventStopDataMask = 0;
    int eventStopMask = 0x3ff00000;
    int eventStopDataValue = 0;
    A_UINT32 MacTraceAddrStart = 0X0;
    A_UINT32 MacTraceAddrEnd = 0X7FF;
    A_UINT32 macTraceBaseRegValue = 0;
    A_UINT32 mac_trace_address_value = ((MacTraceAddrStart << MAC_TRC_ADDR_START_VALUE_LSB) & MAC_TRC_ADDR_START_VALUE_MASK) | ((MacTraceAddrEnd << MAC_TRC_ADDR_END_VALUE_LSB) & MAC_TRC_ADDR_END_VALUE_MASK);
    A_UINT32 testbus_select_address_value = 0;
    A_UINT32 hwsch_debug_select_regvalue = 0;
    A_UINT32 pdg_debug_module_select_regvalue = 0;
    A_UINT32 txdma_tracebus_regvalue = 0;
    A_UINT32 rxdma_tracebus_ctrl_regvalue = 0;
    A_UINT32 ole_debugbus_control_regvalue = 0;
    A_UINT32 crypto_testbus_sel_regvalue = 0;
    A_UINT32 txpcu_tracebus_ctrl_regvalue = 0;
    A_UINT32 rxpcu_test_cfg_regvalue = 0;

    progname = argv[0];

    if (argc == 1) usage();
    flag = 0;
	StopOnEventDataFlag = 0;
	StopOnEventIdxFlag = 0;
    memset(filename, '\0', sizeof(filename));
    memset(devicename, '\0', sizeof(devicename));

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
		{"address", 1, NULL, 'a'},
		{"and", 1, NULL, 'n'},
		{"device", 1, NULL, 'D'},
		{"get", 0, NULL, 'g'},
		{"file", 1, NULL, 'f'},
		{"length", 1, NULL, 'l'},
		{"or", 1, NULL, 'o'},
		{"otp", 0, NULL, 'O'},
		{"param", 1, NULL, 'p'},
		{"quiet", 0, NULL, 'q'},
		{"read", 0, NULL, 'r'},
		{"set", 0, NULL, 's'},
		{"value", 1, NULL, 'p'},
		{"write", 0, NULL, 'w'},
		{"hex", 0, NULL, 'x'},
		{"cal", 0, NULL, 'c'},
		{"count", 0, NULL, 'z'},
		{"phydbg",0, NULL, 'b'},
		{"tracerDump",0, NULL, 'd'},
		{"eventCfg",0, NULL, 'e'},
		{"tracerCfg",0, NULL, 't'},
		{"tracerStart",0, NULL, '2'},
		{"tracerStop",0, NULL, '3'},
		{"tracerClear",0, NULL, '4'},
		{"phydbgCfg",0, NULL, '5'},
		{"phydbgDump",0, NULL, '6'},
		{"phydbgStop",0, NULL, '7'},
		{"smrecorderCfg",0, NULL, '8'},
		{"smrecorderDump",0, NULL, '9'},
		{"phytlv_cnt",0,NULL,'v'},
		{"phyerr_cnt",0,NULL,'u'},
		{"cycle_cnt",0,NULL,'y'},
		{0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "xbrwzgsqtde123456789fvuy:l:a:p:c:n:o:D:",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'b':
            cmd = DIAG_PHYDBG_DUMP;
            break;
        case 'd':
            cmd = DIAG_TRACE_DUMP;
            break;
		case 't':
            cmd = DIAG_TRACE_CFG;
            break;
		case 'e':
            cmd = DIAG_EVENT_CFG;
            break;
		case '2':
            cmd = DIAG_TRACE_START;
            break;
		case '3':
            cmd = DIAG_TRACE_STOP;
            break;
		case '4':
            cmd = DIAG_TRACE_CLEAR;
            break;
		case '5':
			cmd = DIAG_CONFIG_PHYDBG_ADCCAPTURE;
			break;
		case '6':
			cmd = DIAG_DUMP_PHYDBG_ADCCAPTURE;
			break;
		case '7':
			cmd = DIAG_PHYDBG_STOP;
			break;
		case '8':
			cmd = DIAG_BB_SM_RECORDER_CFG;
			break;
		case '9':
			cmd = DIAG_BB_SM_RECORDER_DUMP;
			break;
        case 'r':
            cmd = DIAG_READ_TARGET;
            break;

        case 'w':
            cmd = DIAG_WRITE_TARGET;
            break;

        case 'g':
            cmd = DIAG_READ_WORD;
            break;

        case 's':
            cmd = DIAG_WRITE_WORD;
            break;

        case 'f':
            memset(filename, '\0', sizeof(filename));
            strncpy(filename, optarg, sizeof(filename));
            flag |= FILE_FLAG;
            break;

        case 'l':
            length = parse_address(optarg);
            flag |= LENGTH_FLAG;
            break;

        case 'a':
            address = parse_address(optarg);
            flag |= ADDRESS_FLAG;
            break;

        case 'p':
            param = strtoul(optarg, NULL, 0);
            flag |= PARAM_FLAG;
            break;

        case 'n':
            flag |= PARAM_FLAG | AND_OP_FLAG | BITWISE_OP_FLAG;
            bitwise_mask = strtoul(optarg, NULL, 0);
            break;

        case 'o':
            flag |= PARAM_FLAG | BITWISE_OP_FLAG;
            bitwise_mask = strtoul(optarg, NULL, 0);
            break;

        case 'O':
            flag |= OTP_FLAG;
            break;

        case 'q':
            flag |= QUIET_FLAG;
            break;

        case 'D':
            strncpy(devicename, optarg, sizeof(devicename)-9);
            strcat(devicename, "/athdiag");
            flag |= DEVICE_FLAG;
            break;

        case 'x':
            flag |= HEX_FLAG;
            break;

	case 'c':
	    CAL_FLAG = 1;
	    break;
        case 'z':
            wordcount = parse_address(optarg);
            flag |= WORDCOUNT_FLAG;
            break;
        case 'v':
	   cmd = DIAG_PHYTLV_CNT;
            break;
        case 'u':
            cmd = DIAG_PHYERR_CNT;
            break;
        case 'y':
            cmd = DIAG_CYCLE_CNT;
            break;

        default:
            fprintf(stderr, "Cannot understand '%s'\n", argv[option_index]);
            usage();
        }
    }

    for (;;) {
        /* DIAG uses a sysfs special file which may be auto-detected */
        if (!(flag & DEVICE_FLAG)) {
    	    FILE *find_dev;
    	    size_t nbytes;

            /*
             * Convenience: if no device was specified on the command
             * line, try to figure it out.  Typically there's only a
             * single device anyway.
             */
    	    find_dev = popen("find /sys/devices -name athdiag | head -1", "r");
            if (find_dev) {
    	        nbytes=fread(devicename, 1, sizeof(devicename), find_dev);
    	        pclose(find_dev);

    	        if (nbytes > 15) {
                    /* auto-detect possibly successful */
    	            devicename[nbytes-1]='\0'; /* replace \n with 0 */
    	        } else {
                    strcpy(devicename, "unknown_DIAG_device");
                }
            }
        }

        dev = open(devicename, O_RDWR);
        if (dev >= 0) {
            break; /* successfully opened diag special file */
        } else {
            fprintf(stderr, "err %s failed (%d) to open DIAG file (%s)\n",
                __FUNCTION__, errno, devicename);
            exit(1);
        }
    }
    switch(cmd)
    {
        case DIAG_PHYDBG_DUMP:

            if ((flag & (ADDRESS_FLAG | FILE_FLAG)) ==
                    (ADDRESS_FLAG | FILE_FLAG))
            {
                if ((dump_fd = fopen(filename, "wb+")) < 0)
                {
                    fprintf(stderr, "err %s cannot create/open output file (%s)\n",
                            __FUNCTION__, filename);
                    exit(1);
                }

                bufferWord = (A_UINT32 *)MALLOC(MAX_BUF);
                nqprintf(
                        "DIAG Read Target (address: 0x%x, length: %d, filename: %s)\n",
                        address, length, filename);
                {
                    if(flag & HEX_FLAG)
                    {
                        if (flag & OTP_FLAG) {
                            fprintf(dump_fd,"target otp dump area[0x%08x - 0x%08x]",address,address+length);
                        } else {
                            fprintf(dump_fd,"target mem dump area[0x%08x - 0x%08x]",address,address+length);
                        }
                    }
                    int j = 0;
                    A_UINT32 is_rx_tlv;
                    A_UINT32 tlv_start;
                    A_UINT32 tlv_data;
                    for (j = 0; j <= 10; j++) {
                        ReadTargetWord(dev, address, bufferWord);
                        fprintf(dump_fd,"phydbg[%d] = 0x%08x\n", j, *(A_UINT32 *)bufferWord);
                        is_rx_tlv = ((*bufferWord) >> 31) & 0x1;
                        tlv_start = ((*bufferWord) >> 30) & 0x1;
                        tlv_data = (*bufferWord) & 0x3fffffff;
                        fprintf(dump_fd,"%5d: is_rx_tlv=%d, tlv_start=%d, tlv_data=0x%x\n",j, is_rx_tlv, tlv_start, tlv_data);
                        fprintf(dump_fd,"\n");
                    }
                    }
                }

            fclose(dump_fd);
            free(bufferWord);
            break;
	case DIAG_CONFIG_PHYDBG_ADCCAPTURE:
	    {

		    if ((flag & (FILE_FLAG)) ==
				    FILE_FLAG)
		    {
			    if (!(dump_fd = fopen(filename, "r")))
			    {
				    fprintf(stderr, "err %s cannot open output file (%s)\n",
						    __FUNCTION__, filename);
				    exit(1);
			    }
			    //Processing tracerCfg file BEGIN

			    char buf[1000];
			    int tokenCount=0;

			    char s[] = ": \t";
			    char *token;

			    int lineNum=0;

			    A_UINT32 bb_phydbg_control1_regvalue;
			    A_UINT32 bb_phydbg_control2_regvalue;

			    int phydbg_cap_chn_sel = 0;
			    int phydbg_plybck_count = 0;
			    int phydbg_cap_trig_mode = 1;
			    int phydbg_plybck_trig_mode = 0;
			    int phydbg_cap_pre_store = 0;

			    int phydbg_cap_en = 1;
			    int phydbg_cap_trig_addr = 0;
			    int phydbg_cap_trig_select = 0;
			    int phydbg_fsmstate = 0;
			    int phydbg_apb_autoincr = 1;
			    int phydbg_plybck_en = 0;
			    int phydbg_mode = 5;

			    char line[1000];

			    //bb_phydbg_control1_regvalue = 0x8000;
			    //bb_phydbg_control2_regvalue = 0x80000025;

			    while (fgets(buf,1000, dump_fd)!=NULL)
			    {
				    strncpy(line,buf,1000);
				    lineNum++;
				    if(strtok(line," \t\r\n") == NULL){//if a blank line is found, go to the next line
					    //printf ("Empty line: %d---line: %s\n",lineNum,line);
					    continue;
				    }else{
					    char comment_delim = '#';
					    int index=255;
					    char *ptr = strchr(buf,comment_delim);
					    char *parameter = NULL;
					    A_UINT32 param_value=0;


					    if(ptr){
						    index = ptr - buf;
						    //printf("\'#\'found: at %d in %s",index,ptr);
					    }
					    if(index == 0){//if it's a comment line process the comment to see if it's a delimiter
						    continue;//if comment, go to next line
					    }

					    tokenCount=0;
					    //Remove the trailing new line to help extract event enable
					    strtok(buf,"\n");
					    token = strtok(buf, s);
					    //tsNum = 0;
					    while( token != NULL )
					    {
						    if(tokenCount == 0){
							    parameter = token;//event Num 1st argument in the config file
						    }else if(tokenCount == 1){
							    //mac_trc_ts1_capture_ctrl_cap_mode = strtol(token,NULL,0);//event name ---2nd argument in the config file
							    param_value = strtoul(token,NULL,0);//event name ---2nd argument in the config file
						    }
						    //printf ("tokenCount: %d----tokenStr: %s---tokenInt: %d\n",tokenCount,token,strtol(token,NULL,0));
						    token = strtok(NULL, s);
						    tokenCount++;
					    }

					    if(strcmp(parameter, "PHYDBG_CAP_PRE_STORE") == 0){
						    phydbg_cap_pre_store = param_value;
					    }else if(strcmp(parameter, "PHYDBG_PLYBCK_TRIG_MODE") == 0){
						    phydbg_plybck_trig_mode = param_value;
					    }else if(strcmp(parameter, "PHYDBG_CAP_TRIG_MODE") == 0){
						    phydbg_cap_trig_mode = param_value;
					    }else if(strcmp(parameter, "PHYDBG_PLYBCK_COUNT") == 0){
						    phydbg_plybck_count = param_value;
					    }else if(strcmp(parameter, "PHYDBG_CAP_CHAIN_SEL") == 0){
						    phydbg_cap_chn_sel = param_value;
					    }else if(strcmp(parameter, "PHYDBG_MODE") == 0){
						    phydbg_mode = param_value;
					    }else if(strcmp(parameter, "PHYDBG_PLYBCK_EN") == 0){
						    phydbg_plybck_en = param_value;
					    }else if(strcmp(parameter, "PHYDBG_APB_AUTOINCR") == 0){
						    phydbg_apb_autoincr = param_value;
					    }else if(strcmp(parameter, "PHYDBG_FSMSTATE") == 0){
						    phydbg_fsmstate = param_value;
					    }else if(strcmp(parameter, "PHYDBG_CAP_TRIG_SELECT") == 0){
						    phydbg_cap_trig_select = param_value;
					    }else if(strcmp(parameter, "PHYDBG_CAP_EN") == 0){
						    phydbg_cap_en = param_value;
					    }

				    }
			    }

			    bb_phydbg_control1_regvalue = (( phydbg_cap_chn_sel << BB_PHYDBG_CONTROL1_PHYDBG_CAP_CHN_SEL_LSB) & BB_PHYDBG_CONTROL1_PHYDBG_CAP_CHN_SEL_MASK) | (( phydbg_cap_pre_store << BB_PHYDBG_CONTROL1_PHYDBG_CAP_PRE_STORE_LSB) & BB_PHYDBG_CONTROL1_PHYDBG_CAP_PRE_STORE_MASK) |
				    (( phydbg_cap_trig_mode << BB_PHYDBG_CONTROL1_PHYDBG_CAP_TRIG_MODE_LSB) & BB_PHYDBG_CONTROL1_PHYDBG_CAP_TRIG_MODE_MASK) | (( phydbg_plybck_trig_mode << BB_PHYDBG_CONTROL1_PHYDBG_PLYBCK_TRIG_MODE_LSB) & BB_PHYDBG_CONTROL1_PHYDBG_PLYBCK_TRIG_MODE_MASK) |
				    (( phydbg_plybck_count << BB_PHYDBG_CONTROL1_PHYDBG_PLYBCK_COUNT_LSB) & BB_PHYDBG_CONTROL1_PHYDBG_PLYBCK_COUNT_MASK);

			    bb_phydbg_control2_regvalue = (( phydbg_cap_en << BB_PHYDBG_CONTROL2_PHYDBG_CAP_EN_LSB) & BB_PHYDBG_CONTROL2_PHYDBG_CAP_EN_MASK) | (( phydbg_cap_trig_addr << BB_PHYDBG_CONTROL2_PHYDBG_CAP_TRIG_ADDR_LSB) & BB_PHYDBG_CONTROL2_PHYDBG_CAP_TRIG_ADDR_MASK) |
				    (( phydbg_cap_trig_select << BB_PHYDBG_CONTROL2_PHYDBG_CAP_TRIG_SELECT_LSB) & BB_PHYDBG_CONTROL2_PHYDBG_CAP_TRIG_SELECT_MASK) | (( phydbg_fsmstate << BB_PHYDBG_CONTROL2_PHYDBG_FSMSTATE_LSB) & BB_PHYDBG_CONTROL2_PHYDBG_FSMSTATE_MASK) |
				    (( phydbg_apb_autoincr << BB_PHYDBG_CONTROL2_PHYDBG_APB_AUTOINCR_LSB) & BB_PHYDBG_CONTROL2_PHYDBG_APB_AUTOINCR_MASK) | (( phydbg_plybck_en << BB_PHYDBG_CONTROL2_PHYDBG_PLYBCK_EN_LSB) & BB_PHYDBG_CONTROL2_PHYDBG_PLYBCK_EN_MASK) |
				    (( phydbg_mode << BB_PHYDBG_CONTROL2_PHYDBG_MODE_LSB) & BB_PHYDBG_CONTROL2_PHYDBG_MODE_MASK);
			    A_UINT32 temp;


			    WriteTargetWord(dev,BB_PHYDBG_CONTROL1_REG_ADDR,bb_phydbg_control1_regvalue);
			    WriteTargetWord(dev,BB_PHYDBG_CONTROL2_REG_ADDR,bb_phydbg_control2_regvalue);
			    ReadTargetWord(dev, BB_PHYDBG_CONTROL2_REG_ADDR, &temp);
			    printf("phydbg_ctrl2_value read value: %#x\n",temp);
			    printf("write value: %#x\n",bb_phydbg_control2_regvalue);
			    ReadTargetWord(dev, BB_PHYDBG_CONTROL1_REG_ADDR, &temp);
			    printf("phydbg_ctrl1_value read value: %#x\n",temp);
			    printf("phydbg for adc capture configured!\n");
			    printf("phydbg_mode: %d\n",phydbg_mode);
			    break;
		    }else{
			    usage();
		    }
		    break;
	    }

	case DIAG_BB_SM_RECORDER_CFG:
	{
		A_UINT32 bb_sm_hist_0_regvalue;
		int sm_rec_part_en = 0x9f;
		int sm_rec_chn_en = 0x1;
		int sm_rec_data_num = 0x8;
		int sm_rec_mac_trig = 0x7;

		A_UINT32 bb_gains_min_offsets_regvalue;

		ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
		bb_sm_hist_0_regvalue &= ~BB_SM_HIST_0_SM_REC_EN_MASK;/* clear sm_rec_en bit: If enabled from a prior call*/
		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);

		ReadTargetWord(dev, BB_GAINS_MIN_OFFSETS_REG_ADDR, &bb_gains_min_offsets_regvalue);
		bb_gains_min_offsets_regvalue &= ~(BB_GAINS_MIN_OFFSETS_CF_AGC_HIST_ENABLE_MASK);/*clearing cf_agc_hist_enable bit:
																						Disable AGC History (as it overwrites same memory)*/
		WriteTargetWord(dev,BB_GAINS_MIN_OFFSETS_REG_ADDR,bb_gains_min_offsets_regvalue);
		//ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);

		A_UINT32 bb_chaninfo_ctrl_regvalue;
		ReadTargetWord(dev, BB_CHANINFO_CTRL_REG_ADDR, &bb_chaninfo_ctrl_regvalue);
		bb_chaninfo_ctrl_regvalue &= ~(BB_CHANINFO_CTRL_RTT_HARDWARE_IFFT_MASK & /*clearing both rtt_hardware_iFFT and capture_chan_info bits
																				Disable RTT hardware IFFT (as it overwrites same memory)*/
									  BB_CHANINFO_CTRL_CAPTURE_CHAN_INFO_MASK);/*Disable RTT chan capture  (as it overwrites same memory)*/
		WriteTargetWord(dev,BB_CHANINFO_CTRL_REG_ADDR,bb_chaninfo_ctrl_regvalue);

		A_UINT32 bb_force_clock_regvalue;
		ReadTargetWord(dev, BB_FORCE_CLOCK_REG_ADDR, &bb_force_clock_regvalue);
		bb_force_clock_regvalue &= ~(BB_FORCE_CLOCK_ENA_REG_CLK_GATING_MASK);/*EV 132054 - Disable register clock gating to access chan_info memory*/
		WriteTargetWord(dev,BB_FORCE_CLOCK_REG_ADDR,bb_force_clock_regvalue);

		ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
		bb_sm_hist_0_regvalue &= ~BB_SM_HIST_0_SM_REC_MODE_MASK;/* clear sm_rec_mode bit*/
		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);

		ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
		bb_sm_hist_0_regvalue &= ~BB_SM_HIST_0_SM_REC_TIME_RES_MASK;/* clear sm_rec_time_res bit*/
		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);

		ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
		bb_sm_hist_0_regvalue |= (( sm_rec_part_en << BB_SM_HIST_0_SM_REC_PART_EN_LSB) & BB_SM_HIST_0_SM_REC_PART_EN_MASK);/* write 0x9f to sm_rec_part_en */
		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);

		ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
		bb_sm_hist_0_regvalue |= (( sm_rec_chn_en << BB_SM_HIST_0_SM_REC_CHN_EN_LSB) & BB_SM_HIST_0_SM_REC_CHN_EN_MASK);/* set sm_rec_chn_en bit*/
		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);

		ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
		bb_sm_hist_0_regvalue |= (( sm_rec_data_num << BB_SM_HIST_0_SM_REC_DATA_NUM_LSB) & BB_SM_HIST_0_SM_REC_DATA_NUM_MASK);/* write 0x8 to sm_rec_data_num */
		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);

		ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
		bb_sm_hist_0_regvalue &= ~BB_SM_HIST_0_SM_REC_AGC_SEL_MASK;/* clear sm_rec_agc_sel bit*/
		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);

		ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
		bb_sm_hist_0_regvalue |= (( sm_rec_mac_trig << BB_SM_HIST_0_SM_REC_MAC_TRIG_LSB) & BB_SM_HIST_0_SM_REC_MAC_TRIG_MASK);/* write 0x7 to sm_rec_mac_trig*/
		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);
		/*
		bb_sm_hist_0_regvalue = (( sm_rec_mode << BB_SM_HIST_0_SM_REC_MODE_LSB) & BB_SM_HIST_0_SM_REC_MODE_MASK) |
								(( sm_rec_time_res << BB_SM_HIST_0_SM_REC_TIME_RES_LSB) & BB_SM_HIST_0_SM_REC_TIME_RES_MASK) |
								(( sm_rec_part_en << BB_SM_HIST_0_SM_REC_PART_EN_LSB) & BB_SM_HIST_0_SM_REC_PART_EN_MASK) |
								(( sm_rec_chn_en << BB_SM_HIST_0_SM_REC_CHN_EN_LSB) & BB_SM_HIST_0_SM_REC_CHN_EN_MASK) |
								(( sm_rec_data_num << BB_SM_HIST_0_SM_REC_DATA_NUM_LSB) & BB_SM_HIST_0_SM_REC_DATA_NUM_MASK) |
								(( sm_rec_agc_sel << BB_SM_HIST_0_SM_REC_AGC_SEL_LSB) & BB_SM_HIST_0_SM_REC_AGC_SEL_MASK) |
								(( sm_rec_mac_trig << BB_SM_HIST_0_SM_REC_MAC_TRIG_LSB) & BB_SM_HIST_0_SM_REC_MAC_TRIG_MASK);

		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);
		*/

		WriteTargetWord(dev,BB_CHN_TABLES_INTF_ADDR_REG_ADDR,0xb00);/*tx_plybck_ctrl_0_b1  (To make chan_info memory is writeable)*/
		WriteTargetWord(dev,BB_CHN_TABLES_INTF_DATA_REG_ADDR,0x3);/*Bit0:Enable tx_plyback, Bit1: s2_write (writes upper 16 bits)*/


		int chan_info_addr = 0x3200;/*3200 - 35fc [256 entries]*/
		int loop_cnt;
		for(loop_cnt = 0; loop_cnt <= 255; loop_cnt++){/* Initialize memory to 0*/
			WriteTargetWord(dev,BB_CHN_TABLES_INTF_ADDR_REG_ADDR,chan_info_addr);

			ReadTargetWord(dev, BB_CHANINFO_CTRL_REG_ADDR, &bb_chaninfo_ctrl_regvalue);
			bb_chaninfo_ctrl_regvalue &= ~BB_CHANINFO_CTRL_CHANINFOMEM_S2_READ_MASK; /*clear chaninfomem_s2_read bit*/
			WriteTargetWord(dev,BB_CHANINFO_CTRL_REG_ADDR,bb_chaninfo_ctrl_regvalue);
			WriteTargetWord(dev,BB_CHN_TABLES_INTF_DATA_REG_ADDR,0x0);

			ReadTargetWord(dev, BB_CHANINFO_CTRL_REG_ADDR, &bb_chaninfo_ctrl_regvalue);
			bb_chaninfo_ctrl_regvalue |= BB_CHANINFO_CTRL_CHANINFOMEM_S2_READ_MASK; /*set chaninfomem_s2_read bit*/
			WriteTargetWord(dev,BB_CHANINFO_CTRL_REG_ADDR,bb_chaninfo_ctrl_regvalue);
			WriteTargetWord(dev,BB_CHN_TABLES_INTF_DATA_REG_ADDR,0x0);
			chan_info_addr += 4;
		}

		WriteTargetWord(dev,BB_CHN_TABLES_INTF_ADDR_REG_ADDR,0xb00); /*tx_plybck_ctrl_0_b1  (To make chan_info memory is writeable)*/
		WriteTargetWord(dev,BB_CHN_TABLES_INTF_DATA_REG_ADDR,0x1); /*Bit0:Enable tx_plyback, Bit1: s2_write (writes upper 16 bits)*/

		chan_info_addr = 0x3200;/*3200 - 35fc [256 entries]*/
		for(loop_cnt = 0; loop_cnt <= 255; loop_cnt++){/* Initialize memory to 0*/
			WriteTargetWord(dev,BB_CHN_TABLES_INTF_ADDR_REG_ADDR,chan_info_addr);

			ReadTargetWord(dev, BB_CHANINFO_CTRL_REG_ADDR, &bb_chaninfo_ctrl_regvalue);
			bb_chaninfo_ctrl_regvalue &= ~BB_CHANINFO_CTRL_CHANINFOMEM_S2_READ_MASK; /*clear chaninfomem_s2_read bit*/
			WriteTargetWord(dev,BB_CHANINFO_CTRL_REG_ADDR,bb_chaninfo_ctrl_regvalue);
			WriteTargetWord(dev,BB_CHN_TABLES_INTF_DATA_REG_ADDR,0x0);

			ReadTargetWord(dev, BB_CHANINFO_CTRL_REG_ADDR, &bb_chaninfo_ctrl_regvalue);
			bb_chaninfo_ctrl_regvalue |= BB_CHANINFO_CTRL_CHANINFOMEM_S2_READ_MASK; /*set chaninfomem_s2_read bit*/
			WriteTargetWord(dev,BB_CHANINFO_CTRL_REG_ADDR,bb_chaninfo_ctrl_regvalue);
			WriteTargetWord(dev,BB_CHN_TABLES_INTF_DATA_REG_ADDR,0x0);
			chan_info_addr += 4;
		}

		WriteTargetWord(dev,BB_CHN_TABLES_INTF_ADDR_REG_ADDR,0xb00); /*tx_plybck_ctrl_0_b1 */
		WriteTargetWord(dev,BB_CHN_TABLES_INTF_DATA_REG_ADDR,0x0); /*Memory Write done, disable tx_plyback*/

		ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
		bb_sm_hist_0_regvalue |= BB_SM_HIST_0_SM_REC_EN_MASK; /*Final step: Enable SM recorder*/
		WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);

		printf("sm_recorder configured!\n");
		break;
	}

	case DIAG_BB_SM_RECORDER_DUMP:
	{
		if ((flag & (FILE_FLAG)) ==
				FILE_FLAG)
		{
			if ((dump_fd = fopen(filename, "w")) < 0)
			{
				fprintf(stderr, "err %s cannot create/open output file (%s)\n",
						__FUNCTION__, filename);
				exit(1);
			}

			A_UINT32 bb_chaninfo_ctrl_regvalue;
			A_UINT32 bb_txiqcal_control_0_regvalue;
			A_UINT32 bb_rxiqcal_control_0_regvalue;
			A_UINT32 bb_sm_hist_0_regvalue;
			A_UINT32 bb_sm_hist_1_regvalue;

			ReadTargetWord(dev,BB_CHANINFO_CTRL_REG_ADDR,&bb_chaninfo_ctrl_regvalue);
			fprintf(dump_fd,"BB_chaninfo_ctrl: %#x\n", bb_chaninfo_ctrl_regvalue);

			ReadTargetWord(dev,BB_TXIQCAL_CONTROL_0_REG_ADDR,&bb_txiqcal_control_0_regvalue);
			fprintf(dump_fd,"BB_txiqcal_control_0: %#x\n", bb_txiqcal_control_0_regvalue);

			ReadTargetWord(dev,BB_RXIQCAL_CONTROL_0_REG_ADDR,&bb_rxiqcal_control_0_regvalue);
			fprintf(dump_fd,"BB_rxiqcal_control_0: %#x\n", bb_rxiqcal_control_0_regvalue);

			ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
			fprintf(dump_fd,"BB_sm_hist_0: %#x\n", bb_sm_hist_0_regvalue);

			ReadTargetWord(dev,BB_SM_HIST_1_REG_ADDR,&bb_sm_hist_1_regvalue);
			fprintf(dump_fd,"BB_sm_hist_1: %#x\n", bb_sm_hist_1_regvalue);

			/*clear sm_rec_en bit in bb_sm_hist_0 register*/
			ReadTargetWord(dev,BB_SM_HIST_0_REG_ADDR,&bb_sm_hist_0_regvalue);
			bb_sm_hist_0_regvalue &= ~(BB_SM_HIST_0_SM_REC_EN_MASK);
			WriteTargetWord(dev,BB_SM_HIST_0_REG_ADDR,bb_sm_hist_0_regvalue);

			int last_addr;

			ReadTargetWord(dev,BB_SM_HIST_1_REG_ADDR,&bb_sm_hist_1_regvalue);
			last_addr = (bb_sm_hist_1_regvalue & BB_SM_HIST_1_SM_REC_LAST_ADDR_MASK)>>BB_SM_HIST_1_SM_REC_LAST_ADDR_LSB;
			fprintf(dump_fd,"Last Addr: %d\n", last_addr);

			int short_shift_mode;
			ReadTargetWord(dev,BB_SM_HIST_1_REG_ADDR,&bb_sm_hist_1_regvalue);
			short_shift_mode = (bb_sm_hist_1_regvalue & BB_SM_HIST_1_SM_REC_SS_FORMAT_MASK)>>BB_SM_HIST_1_SM_REC_SS_FORMAT_LSB;
			fprintf(dump_fd,"short_shift_mode: %d\n", short_shift_mode);

			int loop_cnt;
			A_UINT32 lower_word;
			A_UINT32 upper_word;
			int chan_info_addr = 0x3200;
			for(loop_cnt=0;loop_cnt<=255;loop_cnt++){

				WriteTargetWord(dev,BB_CHN_TABLES_INTF_ADDR_REG_ADDR,chan_info_addr);

				ReadTargetWord(dev, BB_CHANINFO_CTRL_REG_ADDR, &bb_chaninfo_ctrl_regvalue);
				bb_chaninfo_ctrl_regvalue &= ~BB_CHANINFO_CTRL_CHANINFOMEM_S2_READ_MASK; /*clear chaninfomem_s2_read bit*/
				WriteTargetWord(dev,BB_CHANINFO_CTRL_REG_ADDR,bb_chaninfo_ctrl_regvalue);

				ReadTargetWord(dev,BB_CHN_TABLES_INTF_DATA_REG_ADDR,&lower_word);

				ReadTargetWord(dev, BB_CHANINFO_CTRL_REG_ADDR, &bb_chaninfo_ctrl_regvalue);
				bb_chaninfo_ctrl_regvalue |= BB_CHANINFO_CTRL_CHANINFOMEM_S2_READ_MASK; /*set chaninfomem_s2_read bit*/
				WriteTargetWord(dev,BB_CHANINFO_CTRL_REG_ADDR,bb_chaninfo_ctrl_regvalue);

				ReadTargetWord(dev,BB_CHN_TABLES_INTF_DATA_REG_ADDR,&upper_word);

				fprintf(dump_fd,"%d: raw word = [%#x %#x]\n", loop_cnt, upper_word, lower_word);

				chan_info_addr = chan_info_addr + 4;
			}

		}
		break;
	}

	case DIAG_PHYDBG_STOP:
	{
			int phydbg_cap_en = 1;
			A_UINT32 bb_phydbg_control2_regvalue;

			ReadTargetWord(dev, BB_PHYDBG_CONTROL2_REG_ADDR,&bb_phydbg_control2_regvalue);
			printf("bb_phydbg_control2_regvalue: %#x",bb_phydbg_control2_regvalue);
			//clear phydbg_cap_en bit for tlv capture
			bb_phydbg_control2_regvalue &= ~( phydbg_cap_en << BB_PHYDBG_CONTROL2_PHYDBG_CAP_EN_LSB);
			printf("write value: %#x\n",bb_phydbg_control2_regvalue);
			WriteTargetWord(dev,BB_PHYDBG_CONTROL2_REG_ADDR,bb_phydbg_control2_regvalue);

			//ReadTargetWord(dev, BB_PHYDBG_CONTROL2_REG_ADDR, &bb_phydbg_control2_regvalue);
			//printf("read value: %#x\n",bb_phydbg_control2_regvalue);
			printf("phydbg stopped: disabled phydbg_cap_en\n");
		break;
	}

	case DIAG_DUMP_PHYDBG_ADCCAPTURE:
	{
	if ((flag & (FILE_FLAG)) ==
                FILE_FLAG)
		{
			if ((dump_fd = fopen(filename, "w")) < 0)
			{
				fprintf(stderr, "err %s cannot create/open output file (%s)\n",
                    __FUNCTION__, filename);
				exit(1);
			}
			WriteTargetWord(dev,BB_PHYDBG_MEM_ADDR,0);
			int dump_phydbg_cnt = 0;
			int phydbg_mode;
			int phydbg_cap_chn_sel;

			A_UINT32 payload;
			ReadTargetWord(dev, BB_PHYDBG_CONTROL1_REG_ADDR, &payload);
			phydbg_cap_chn_sel = (payload & BB_PHYDBG_CONTROL1_PHYDBG_CAP_CHN_SEL_MASK)>>BB_PHYDBG_CONTROL1_PHYDBG_CAP_CHN_SEL_LSB;
			fprintf(dump_fd,"chain_id: %#x\n",phydbg_cap_chn_sel);
			ReadTargetWord(dev, BB_PHYDBG_CONTROL2_REG_ADDR, &payload);
			phydbg_mode = (payload & BB_PHYDBG_CONTROL2_PHYDBG_MODE_MASK)>>BB_PHYDBG_CONTROL2_PHYDBG_MODE_LSB;
			fprintf(dump_fd,"phydbg_mode: %#x\n",phydbg_mode);
			for (dump_phydbg_cnt=0;dump_phydbg_cnt<=23935; dump_phydbg_cnt++){
				ReadTargetWord(dev, BB_PHYDBG_MEM_DATA, &payload);
				fprintf(dump_fd,"%d: %#x\n", dump_phydbg_cnt,payload);
			}
			fclose(dump_fd);
//		ReadTargetWord(dev, BB_PHYDBG_CONTROL2_REG_ADDR, &phydbg_ctrl2_value);
//		printf("write value: %#x\n",phydbg_ctrl2_value);
		}else{
			usage();
		}
		break;
	}
    case DIAG_TRACE_DUMP:

        if ((flag & FILE_FLAG) ==
                FILE_FLAG)
        {
            if ((dump_fd = fopen(filename, "wb+")) < 0)
            {
                fprintf(stderr, "err %s cannot create/open output file (%s)\n",
                        __FUNCTION__, filename);
                exit(1);
            }
			address = 0x31c64;
            bufferWordLSB = (A_UINT32 *)MALLOC(MAX_BUF);
            bufferWordMSB = (A_UINT32 *)MALLOC(MAX_BUF);
            nqprintf(
                    "DIAG Read Target (address: 0x%x, length: %d, filename: %s)\n",
                    address, length, filename);
            {
                if(flag & HEX_FLAG)
                {
                    if (flag & OTP_FLAG) {
                        fprintf(dump_fd,"target otp dump area[0x%08x - 0x%08x]",address,address+length);
                    } else {
                        fprintf(dump_fd,"target mem dump area[0x%08x - 0x%08x]",address,address+length);
                    }
                }
                int j = 0;
				int bufMemCount = 0;
                A_UINT32 trace_offset;
                A_UINT32 testbus_select;
                A_UINT32 event_bus_select;
                A_UINT32 hwsch_dbg_select;
                A_UINT32 pdg_dbg_select;
                A_UINT32 txdma_dbg_select;
                A_UINT32 ole_dbg_select;
                A_UINT32 crypto_dbg_select;
                A_UINT32 txpcu_dbg_select;
                A_UINT32 rxpcu_dbg_select;
                A_UINT32 rxdma_dbg_select;
				A_UINT32 bufferReadStartAddr;
				A_UINT32 bufferReadEndAddr;
				//A_UINT32 bufferReadAddr;



				ReadTargetWord(dev,__MAC_TRC_REG_BASE_ADDRESS, &macTraceBaseRegValue);
				macTraceBaseRegValue |= ((1 << MAC_TRC_CPU_ACCESS_EN_VALUE_LSB) & MAC_TRC_CPU_ACCESS_EN_VALUE_MASK);
				WriteTargetWord(dev,__MAC_TRC_REG_BASE_ADDRESS, macTraceBaseRegValue);
				//Read this register before disabling the tracer, else, startAddr will lose value
				ReadTargetWord(dev, MAC_TRC_STATUS_1_ADDRESS, &bufferReadAddr);

                ReadTargetWord(dev, 0x311e0, &testbus_select);
                fprintf(dump_fd,"TESTBUS_SELECT = 0x%08x\n", testbus_select);
                ReadTargetWord(dev, MCMN_EVENT_TRACE_BUS_SELECT_ADDRESS, &event_bus_select);
                fprintf(dump_fd,"MCMN_EVENT_TRACE_BUS_SELECT = 0x%08x\n", event_bus_select);

				switch(testbus_select)
				{
				case 0x10:
				ReadTargetWord(dev, 0x3f738, &hwsch_dbg_select);
                fprintf(dump_fd,"HWSCH_DEBUG_SELECT = 0x%08x\n", hwsch_dbg_select);
				break;

				case 0x11:
				ReadTargetWord(dev, 0x30024, &pdg_dbg_select);
                fprintf(dump_fd,"PDG_DEBUG_SELECT = 0x%08x\n", pdg_dbg_select);
                break;

				case 0x12:
                ReadTargetWord(dev, 0x30410, &txdma_dbg_select);
				fprintf(dump_fd,"TXDMA_DEBUG_SELECT = 0x%08x\n", txdma_dbg_select);
				break;

				case 0x13:
                ReadTargetWord(dev, 0x308cc, &rxdma_dbg_select);
                fprintf(dump_fd,"RXDMA_DEBUG_SELECT = 0x%08x\n", rxdma_dbg_select);
				break;

				case 0x14:
				ReadTargetWord(dev, 0x30d2c, &ole_dbg_select);
                fprintf(dump_fd,"OLE_DEBUG_SELECT = 0x%08x\n", ole_dbg_select);
				break;

				case 0x15:
				ReadTargetWord(dev, 0x36080, &txpcu_dbg_select);
                fprintf(dump_fd,"TXPCU_DEBUG_SELECT = 0x%08x\n", txpcu_dbg_select);
				break;

				case 0x16:
                ReadTargetWord(dev, 0x32160, &rxpcu_dbg_select);
                fprintf(dump_fd,"RXPCU_DEBUG_SELECT = 0x%08x\n", rxpcu_dbg_select);
				break;

				case 0x17:
				ReadTargetWord(dev, 0x3f01c, &crypto_dbg_select);
                fprintf(dump_fd,"CRYPTO_DEBUG_SELECT = 0x%08x\n", crypto_dbg_select);
				break;

				default:
				break;
                }
				//Read for BUF_RD_START and END---dump memory between the start and end addresses
				//ReadTargetWord(dev, MAC_TRC_STATUS_1_ADDRESS, &bufferReadAddr);
				bufferReadStartAddr = (bufferReadAddr & MAC_TRC_STATUS_1_BUF_RD_START_ADDR_MASK)>>MAC_TRC_STATUS_1_BUF_RD_START_ADDR_LSB;
				bufferReadEndAddr = (bufferReadAddr & MAC_TRC_STATUS_1_BUF_RD_END_ADDR_MASK)>>MAC_TRC_STATUS_1_BUF_RD_END_ADDR_LSB;

				//Read MAC_TRC_ADDR---Trace Memory Address Range
				ReadTargetWord(dev, MAC_TRC_ADDR_ADDRESS, &mac_trace_address_value);
				MacTraceAddrStart = (mac_trace_address_value & MAC_TRC_ADDR_START_VALUE_MASK)>>MAC_TRC_ADDR_START_VALUE_LSB;
				MacTraceAddrEnd = (mac_trace_address_value & MAC_TRC_ADDR_END_VALUE_MASK)>>MAC_TRC_ADDR_END_VALUE_LSB;

				printf("BufferAddrStart: %#x AddrEnd: %#x\n",bufferReadStartAddr,bufferReadEndAddr);
				printf("macTraceAddrStart: %#x AddrEnd: %#x\n",MacTraceAddrStart,MacTraceAddrEnd);

				j = bufferReadStartAddr;
				if (bufferReadStartAddr == (bufferReadEndAddr + 1)){//buffer memory wrap around
				printf("inside wraparound\n");
					for(bufMemCount = 0; bufMemCount <= (MacTraceAddrEnd-MacTraceAddrStart); bufMemCount++){
						if (j > MacTraceAddrEnd){
							j=MacTraceAddrStart;
						}
						WriteTargetWord(dev,0x31c64, j);
						WriteTargetWord(dev,0x31c80, 0x0);
						ReadTargetWord(dev, 0x31c64, &trace_offset);
						ReadTargetWord(dev, 0x31c68, bufferWordLSB);
						fprintf(dump_fd,"TRC: 0x%x LOW: 0x%08x ", trace_offset,*(A_UINT32 *)bufferWordLSB);
						WriteTargetWord(dev,0x31c80, 0x1);
						ReadTargetWord(dev, 0x31c68, bufferWordMSB);
						fprintf(dump_fd,"HIGH: 0x%x MemoryLocation: %#x\n",*(A_UINT32 *)bufferWordMSB,j);
						j++;

					}
				}else{
				printf("inside singleshot\n");
					for (j = bufferReadStartAddr; j <= bufferReadEndAddr; j++) {
						WriteTargetWord(dev,0x31c64, j);
						WriteTargetWord(dev,0x31c80, 0x0);
						ReadTargetWord(dev, 0x31c64, &trace_offset);
						ReadTargetWord(dev, 0x31c68, bufferWordLSB);
						fprintf(dump_fd,"TRC: 0x%x LOW: 0x%08x ", trace_offset,*(A_UINT32 *)bufferWordLSB);
						WriteTargetWord(dev,0x31c80, 0x1);
						ReadTargetWord(dev, 0x31c68, bufferWordMSB);
						fprintf(dump_fd,"HIGH: 0x%x MemoryLocation: %#x\n",*(A_UINT32 *)bufferWordMSB,j);
					}
				}
            }

        fclose(dump_fd);
        free(bufferWordLSB);
        free(bufferWordMSB);
		}else{
            usage();
        }
        break;
	case DIAG_TRACE_CLEAR:
		printf("clearing trace buffer---wait for 3 seconds\n");
		WriteTargetWord(dev,MAC_TRC_ADDR_ADDRESS, 0x7ff0000);
		WriteTargetWord(dev,__MAC_TRC_REG_BASE_ADDRESS, 0x0);
		WriteTargetWord(dev,MAC_TRC_BUF_INIT_ADDRESS, 0x1);
		sleep(3);
		WriteTargetWord(dev,MAC_TRC_BUF_INIT_ADDRESS, 0x0);
		break;
	case DIAG_TRACE_CFG:
		if ((flag & (FILE_FLAG)) ==
                FILE_FLAG)
		{
			if ((dump_fd = fopen(filename, "r")) < 0)
			{
				fprintf(stderr, "err %s cannot create/open output file (%s)\n",
                    __FUNCTION__, filename);
				exit(1);
			}
			//Processing tracerCfg file BEGIN

			char buf[1000];
			int tokenCount=0;

			char s[] = ": \t";
			char *token;

			int lineNum=0;

			A_UINT32 mac_trc_time_stamp_regValue = 0;
			A_UINT32 mac_trc_lane_swap_regValue = 0;
			A_UINT32 testbus_select_tracer_sel_value = 0;
			A_UINT32 testbus_select_wmac_sel_value = 0;


			char line[1000];

			while (fgets(buf,1000, dump_fd)!=NULL)
			{	strcpy(line,buf);
				lineNum++;
				if(strtok(line," \t\r\n") == NULL){//if a blank line is found, go to the next line
				//printf ("Empty line: %d---line: %s\n",lineNum,line);
				continue;
				}else{
					char comment_delim = '#';
					int index=255;
					char *ptr = strchr(buf,comment_delim);
					char *parameter = NULL;
					A_UINT32 param_value=0;


					if(ptr){
						index = ptr - buf;
						//printf("\'#\'found: at %d in %s",index,ptr);
					}
					if(index == 0){//if it's a comment line process the comment to see if it's a delimiter
						continue;//if comment, go to next line
					}

						tokenCount=0;
						//Remove the trailing new line to help extract event enable
						strtok(buf,"\n");
						token = strtok(buf, s);
							//tsNum = 0;
					   while( token != NULL )
						{
							if(tokenCount == 0){
								parameter = token;//event Num 1st argument in the config file
							}else if(tokenCount == 1){
								//mac_trc_ts1_capture_ctrl_cap_mode = strtol(token,NULL,0);//event name ---2nd argument in the config file
								param_value = strtoul(token,NULL,0);//event name ---2nd argument in the config file
							}
							//printf ("tokenCount: %d----tokenStr: %s---tokenInt: %d\n",tokenCount,token,strtol(token,NULL,0));
							token = strtok(NULL, s);
							tokenCount++;
						}

						if(strcmp(parameter, "MAC_TRC_TS1_CAPTURE_CTRL_CAP_MODE") == 0){//TS1 registers
							mac_trc_ts[0][0].regValue |= (( param_value << MAC_TRC_TS1_CAPTURE_CTRL_CAP_MODE_LSB) & MAC_TRC_TS1_CAPTURE_CTRL_CAP_MODE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS1_CAPTURE_CTRL_CAP_MASK") == 0){
							mac_trc_ts[0][0].regValue |= (( param_value << MAC_TRC_TS1_CAPTURE_CTRL_CAP_MASK_LSB) & MAC_TRC_TS1_CAPTURE_CTRL_CAP_MASK_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS1_STATE_CTRL_MAX_CAP_CNT") == 0){
							mac_trc_ts[0][1].regValue |= (( param_value << MAC_TRC_TS1_STATE_CTRL_MAX_CAP_CNT_LSB) & MAC_TRC_TS1_STATE_CTRL_MAX_CAP_CNT_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS1_STATE_CTRL_NXT_TS_EN") == 0){
							mac_trc_ts[0][1].regValue |= (( param_value << MAC_TRC_TS1_STATE_CTRL_NXT_TS_EN_LSB) & MAC_TRC_TS1_STATE_CTRL_NXT_TS_EN_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS1_TRIG_MASK_VALUE") == 0){
							mac_trc_ts[0][2].regValue = (( param_value << MAC_TRC_TS1_TRIG_MASK_VALUE_LSB) & MAC_TRC_TS1_TRIG_MASK_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS1_TRIG_VALUE") == 0){
							mac_trc_ts[0][3].regValue = (( param_value << MAC_TRC_TS1_TRIG_VALUE_LSB) & MAC_TRC_TS1_TRIG_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS2_CAPTURE_CTRL_CAP_MODE") == 0){//TS2 registers
							mac_trc_ts[1][0].regValue |= (( param_value << MAC_TRC_TS2_CAPTURE_CTRL_CAP_MODE_LSB) & MAC_TRC_TS2_CAPTURE_CTRL_CAP_MODE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS2_CAPTURE_CTRL_CAP_MASK") == 0){
							mac_trc_ts[1][0].regValue |= (( param_value << MAC_TRC_TS2_CAPTURE_CTRL_CAP_MASK_LSB) & MAC_TRC_TS2_CAPTURE_CTRL_CAP_MASK_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS2_STATE_CTRL_MAX_CAP_CNT") == 0){
							mac_trc_ts[1][1].regValue |= (( param_value << MAC_TRC_TS2_STATE_CTRL_MAX_CAP_CNT_LSB) & MAC_TRC_TS2_STATE_CTRL_MAX_CAP_CNT_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS2_STATE_CTRL_NXT_TS_EN") == 0){
							mac_trc_ts[1][1].regValue |= (( param_value << MAC_TRC_TS2_STATE_CTRL_NXT_TS_EN_LSB) & MAC_TRC_TS2_STATE_CTRL_NXT_TS_EN_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS2_TRIG_MASK_VALUE") == 0){
							mac_trc_ts[1][2].regValue = (( param_value << MAC_TRC_TS2_TRIG_MASK_VALUE_LSB) & MAC_TRC_TS2_TRIG_MASK_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS2_TRIG_VALUE") == 0){
							mac_trc_ts[1][3].regValue = (( param_value << MAC_TRC_TS2_TRIG_VALUE_LSB) & MAC_TRC_TS2_TRIG_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS3_CAPTURE_CTRL_CAP_MODE") == 0){//TS3 registers
							mac_trc_ts[2][0].regValue |= (( param_value << MAC_TRC_TS3_CAPTURE_CTRL_CAP_MODE_LSB) & MAC_TRC_TS3_CAPTURE_CTRL_CAP_MODE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS3_CAPTURE_CTRL_CAP_MASK") == 0){
							mac_trc_ts[2][0].regValue |= (( param_value << MAC_TRC_TS3_CAPTURE_CTRL_CAP_MASK_LSB) & MAC_TRC_TS3_CAPTURE_CTRL_CAP_MASK_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS3_STATE_CTRL_MAX_CAP_CNT") == 0){
							mac_trc_ts[2][1].regValue |= (( param_value << MAC_TRC_TS3_STATE_CTRL_MAX_CAP_CNT_LSB) & MAC_TRC_TS3_STATE_CTRL_MAX_CAP_CNT_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS3_STATE_CTRL_NXT_TS_EN") == 0){
							mac_trc_ts[2][1].regValue |= (( param_value << MAC_TRC_TS3_STATE_CTRL_NXT_TS_EN_LSB) & MAC_TRC_TS3_STATE_CTRL_NXT_TS_EN_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS3_TRIG_MASK_VALUE") == 0){
							mac_trc_ts[2][2].regValue = (( param_value << MAC_TRC_TS3_TRIG_MASK_VALUE_LSB) & MAC_TRC_TS3_TRIG_MASK_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS3_TRIG_VALUE") == 0){
							mac_trc_ts[2][3].regValue = (( param_value << MAC_TRC_TS3_TRIG_VALUE_LSB) & MAC_TRC_TS3_TRIG_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS4_CAPTURE_CTRL_CAP_MODE") == 0){//TS4 registers
							mac_trc_ts[3][0].regValue |= (( param_value << MAC_TRC_TS4_CAPTURE_CTRL_CAP_MODE_LSB) & MAC_TRC_TS4_CAPTURE_CTRL_CAP_MODE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS4_CAPTURE_CTRL_CAP_MASK") == 0){
							mac_trc_ts[3][0].regValue |= (( param_value << MAC_TRC_TS4_CAPTURE_CTRL_CAP_MASK_LSB) & MAC_TRC_TS4_CAPTURE_CTRL_CAP_MASK_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS4_STATE_CTRL_MAX_CAP_CNT") == 0){
							mac_trc_ts[3][1].regValue |= (( param_value << MAC_TRC_TS4_STATE_CTRL_MAX_CAP_CNT_LSB) & MAC_TRC_TS4_STATE_CTRL_MAX_CAP_CNT_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS4_STATE_CTRL_NXT_TS_EN") == 0){
							mac_trc_ts[3][1].regValue |= (( param_value << MAC_TRC_TS4_STATE_CTRL_NXT_TS_EN_LSB) & MAC_TRC_TS4_STATE_CTRL_NXT_TS_EN_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS4_TRIG_MASK_VALUE") == 0){
							mac_trc_ts[3][2].regValue = (( param_value << MAC_TRC_TS4_TRIG_MASK_VALUE_LSB) & MAC_TRC_TS4_TRIG_MASK_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS4_TRIG_VALUE") == 0){
							mac_trc_ts[3][3].regValue = (( param_value << MAC_TRC_TS4_TRIG_VALUE_LSB) & MAC_TRC_TS4_TRIG_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS5_CAPTURE_CTRL_CAP_MODE") == 0){//TS5 registers
							mac_trc_ts[4][0].regValue |= (( param_value << MAC_TRC_TS5_CAPTURE_CTRL_CAP_MODE_LSB) & MAC_TRC_TS5_CAPTURE_CTRL_CAP_MODE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS5_CAPTURE_CTRL_CAP_MASK") == 0){
							mac_trc_ts[4][0].regValue |= (( param_value << MAC_TRC_TS5_CAPTURE_CTRL_CAP_MASK_LSB) & MAC_TRC_TS5_CAPTURE_CTRL_CAP_MASK_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS5_STATE_CTRL_MAX_CAP_CNT") == 0){
							mac_trc_ts[4][1].regValue |= (( param_value << MAC_TRC_TS5_STATE_CTRL_MAX_CAP_CNT_LSB) & MAC_TRC_TS5_STATE_CTRL_MAX_CAP_CNT_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS5_STATE_CTRL_NXT_TS_EN") == 0){
							mac_trc_ts[4][1].regValue |= (( param_value << MAC_TRC_TS5_STATE_CTRL_NXT_TS_EN_LSB) & MAC_TRC_TS5_STATE_CTRL_NXT_TS_EN_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS5_TRIG_MASK_VALUE") == 0){
							mac_trc_ts[4][2].regValue = (( param_value << MAC_TRC_TS5_TRIG_MASK_VALUE_LSB) & MAC_TRC_TS5_TRIG_MASK_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TS5_TRIG_VALUE") == 0){
							mac_trc_ts[4][3].regValue = (( param_value << MAC_TRC_TS5_TRIG_VALUE_LSB) & MAC_TRC_TS5_TRIG_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_WFT_CAPTURE_CTRL_CAP_MODE") == 0){//WFT registers
							mac_trc_wft[0].regValue |= (( param_value << MAC_TRC_WFT_CAPTURE_CTRL_CAP_MODE_LSB) & MAC_TRC_WFT_CAPTURE_CTRL_CAP_MODE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_WFT_CAPTURE_CTRL_CAP_MASK") == 0){
							mac_trc_wft[0].regValue |= (( param_value << MAC_TRC_WFT_CAPTURE_CTRL_CAP_MASK_LSB) & MAC_TRC_WFT_CAPTURE_CTRL_CAP_MASK_MASK);
						}else if(strcmp(parameter, "MAC_TRC_WFT_STATE_CTRL_MAX_CAP_CNT") == 0){
							mac_trc_wft[1].regValue |= (( param_value << MAC_TRC_WFT_STATE_CTRL_MAX_CAP_CNT_LSB) & MAC_TRC_WFT_STATE_CTRL_MAX_CAP_CNT_MASK);
						}else if(strcmp(parameter, "MAC_TRC_WFT_STATE_CTRL_NXT_TS_EN") == 0){
							mac_trc_wft[1].regValue |= (( param_value << MAC_TRC_WFT_STATE_CTRL_NXT_TS_EN_LSB) & MAC_TRC_WFT_STATE_CTRL_NXT_TS_EN_MASK);
						}else if(strcmp(parameter, "MAC_TRC_TIME_STAMP_SEL") == 0){//TimeStamp
							mac_trc_time_stamp_regValue = (( param_value << MAC_TRC_TIME_STAMP_SEL_LSB) & MAC_TRC_TIME_STAMP_SEL_MASK);
						}else if(strcmp(parameter, "MAC_TRC_LANE_SWAP_SEL") == 0){//LaneSwap
							mac_trc_lane_swap_regValue = (( param_value << MAC_TRC_LANE_SWAP_SEL_LSB) & MAC_TRC_LANE_SWAP_SEL_MASK);
						}else if(strcmp(parameter, "MAC_TRC_BUF_CFG_VALUE") == 0){//RingOrFixedBuffer
							macTraceBaseRegValue |= (( param_value << MAC_TRC_BUF_CFG_VALUE_LSB) & MAC_TRC_BUF_CFG_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_ADDR_START_VALUE") == 0){//MacTraceAddrStart
							MacTraceAddrStart =  param_value;
						}else if(strcmp(parameter, "MAC_TRC_ADDR_END_VALUE") == 0){//MacTraceAddrEnd
							MacTraceAddrEnd =  param_value;
						}else if(strcmp(parameter, "TESTBUS_SELECT_TRACER_SEL") == 0){//MacTraceAddrEnd
							testbus_select_tracer_sel_value = param_value;
						}else if(strcmp(parameter, "TESTBUS_SELECT_WMAC_SEL") == 0){//MacTraceAddrEnd
							testbus_select_wmac_sel_value = param_value;
						}else if(strcmp(parameter, "HWSCH_DEBUG_SELECT") == 0){//MacTraceAddrEnd
							hwsch_debug_select_regvalue = param_value;
						}else if(strcmp(parameter, "PDG_DEBUG_MODULE_SELECT") == 0){//MacTraceAddrEnd
							pdg_debug_module_select_regvalue = param_value;
						}else if(strcmp(parameter, "TXDMA_TRACEBUS") == 0){//MacTraceAddrEnd
							txdma_tracebus_regvalue = param_value;
						}else if(strcmp(parameter, "RXDMA_TRACEBUS_CTRL") == 0){//MacTraceAddrEnd
							rxdma_tracebus_ctrl_regvalue = param_value;
						}else if(strcmp(parameter, "OLE_DEBUGBUS_CONTROL") == 0){//MacTraceAddrEnd
							ole_debugbus_control_regvalue = param_value;
						}else if(strcmp(parameter, "CRYPTO_TESTBUS_SEL") == 0){//MacTraceAddrEnd
							crypto_testbus_sel_regvalue = param_value;
						}else if(strcmp(parameter, "TXPCU_TRACEBUS_CTRL") == 0){//MacTraceAddrEnd
							txpcu_tracebus_ctrl_regvalue = param_value;
						}else if(strcmp(parameter, "RXPCU_TEST_CFG") == 0){//MacTraceAddrEnd
							rxpcu_test_cfg_regvalue = param_value;
						}
						//printf("parameter: %s param_value: %#x\n",parameter,param_value);
						continue;
				}
			}//while end---tracer_config read end
				mac_trace_address_value = ((MacTraceAddrStart << MAC_TRC_ADDR_START_VALUE_LSB) & MAC_TRC_ADDR_START_VALUE_MASK) | ((MacTraceAddrEnd << MAC_TRC_ADDR_END_VALUE_LSB) & MAC_TRC_ADDR_END_VALUE_MASK);
				testbus_select_address_value = (( testbus_select_tracer_sel_value << TESTBUS_SELECT_TRACER_SEL_LSB) & TESTBUS_SELECT_TRACER_SEL_MASK) | (( testbus_select_wmac_sel_value << TESTBUS_SELECT_WMAC_SEL_LSB) & TESTBUS_SELECT_WMAC_SEL_MASK);;

				printf("wft: capture_ctrl_reg_value: %#x state_ctrl_reg_value: %#x\n",mac_trc_wft[0].regValue,mac_trc_wft[1].regValue);
				printf("mac_trc_time_stamp_regValue: %#x mac_trc_lane_swap_regValue: %#x\n",mac_trc_time_stamp_regValue,mac_trc_lane_swap_regValue);
				printf("macTraceBaseRegValue: %#x mac_trace_address_value: %#x\n",macTraceBaseRegValue,mac_trace_address_value);

				printf("testbus_select_address_value: %#x \n",testbus_select_address_value);
				printf("TS1: capture_ctrl_reg_value: %#x state_ctrl_reg_value: %#x trig_mask_reg_value: %#x trig_reg_value: %#x\n",mac_trc_ts[0][0].regValue,mac_trc_ts[0][1].regValue
																																		,mac_trc_ts[0][2].regValue,mac_trc_ts[0][3].regValue);
				printf("TS2: capture_ctrl_reg_value: %#x state_ctrl_reg_value: %#x trig_mask_reg_value: %#x trig_reg_value: %#x\n",mac_trc_ts[1][0].regValue,mac_trc_ts[1][1].regValue
																																		,mac_trc_ts[1][2].regValue,mac_trc_ts[1][3].regValue);
				printf("TS3: capture_ctrl_reg_value: %#x state_ctrl_reg_value: %#x trig_mask_reg_value: %#x trig_reg_value: %#x\n",mac_trc_ts[2][0].regValue,mac_trc_ts[2][1].regValue
																																		,mac_trc_ts[2][2].regValue,mac_trc_ts[2][3].regValue);
				printf("TS4: capture_ctrl_reg_value: %#x state_ctrl_reg_value: %#x trig_mask_reg_value: %#x trig_reg_value: %#x\n",mac_trc_ts[3][0].regValue,mac_trc_ts[3][1].regValue
																																		,mac_trc_ts[3][2].regValue,mac_trc_ts[3][3].regValue);
				printf("TS5: capture_ctrl_reg_value: %#x state_ctrl_reg_value: %#x trig_mask_reg_value: %#x trig_reg_value: %#x\n",mac_trc_ts[4][0].regValue,mac_trc_ts[4][1].regValue
																																		,mac_trc_ts[4][2].regValue,mac_trc_ts[4][3].regValue);

				printf("hwsch_debug_select_regvalue: %#x pdg_debug_module_select_regvalue: %#x\n",hwsch_debug_select_regvalue,pdg_debug_module_select_regvalue);
				printf("txdma_tracebus_regvalue: %#x rxdma_tracebus_ctrl_regvalue: %#x\n",txdma_tracebus_regvalue,rxdma_tracebus_ctrl_regvalue);
				printf("ole_debugbus_control_regvalue: %#x crypto_testbus_sel_regvalue: %#x\n",ole_debugbus_control_regvalue,crypto_testbus_sel_regvalue);
				printf("txpcu_tracebus_ctrl_regvalue: %#x rxpcu_test_cfg_regvalue: %#x\n",txpcu_tracebus_ctrl_regvalue,rxpcu_test_cfg_regvalue);
				fclose(dump_fd);

			WriteTargetWord(dev,MCMN_EVENT_TRACE_BUS_SELECT_ADDRESS, 0x0);

			WriteTargetWord(dev,TESTBUS_SELECT_ADDRESS, testbus_select_address_value);
			WriteTargetWord(dev,HWSCH_DEBUG_SELECT_ADDRESS, hwsch_debug_select_regvalue);
			WriteTargetWord(dev,PDG_DEBUG_MODULE_SELECT_ADDRESS, pdg_debug_module_select_regvalue);
			WriteTargetWord(dev,TXDMA_TRACEBUS_ADDRESS, txdma_tracebus_regvalue);
			WriteTargetWord(dev,RXDMA_TRACEBUS_CTRL_ADDRESS, rxdma_tracebus_ctrl_regvalue);
			WriteTargetWord(dev,OLE_DEBUGBUS_CONTROL_ADDRESS, ole_debugbus_control_regvalue);
			WriteTargetWord(dev,CRYPTO_TESTBUS_SEL_ADDRESS, crypto_testbus_sel_regvalue);
			WriteTargetWord(dev,TXPCU_TRACEBUS_CTRL_ADDRESS, txpcu_tracebus_ctrl_regvalue);
			WriteTargetWord(dev,RXPCU_TEST_CFG_ADDRESS, rxpcu_test_cfg_regvalue);

			//to enable timestamp
			WriteTargetWord(dev,MAC_TRC_TIME_STAMP_ADDRESS, mac_trc_time_stamp_regValue);

			//bit 1 if set to 1---wrap around, else it's single shot
			WriteTargetWord(dev,__MAC_TRC_REG_BASE_ADDRESS, macTraceBaseRegValue);

			//MAC_TRC_ADDR---0:13---start addr, 16:29---end addr
			//mac_trace_address_value = ((MacTraceAddrStart << MAC_TRC_ADDR_START_VALUE_LSB) & MAC_TRC_ADDR_START_VALUE_MASK) | ((MacTraceAddrEnd << MAC_TRC_ADDR_END_VALUE_LSB) & MAC_TRC_ADDR_END_VALUE_MASK);
			printf("%#x mac_trace_address_value: %#x\n",MAC_TRC_ADDR_ADDRESS,mac_trace_address_value);
			WriteTargetWord(dev,MAC_TRC_ADDR_ADDRESS, mac_trace_address_value);
			//MAC_TRC_LANE_SWAP
			WriteTargetWord(dev,MAC_TRC_LANE_SWAP_ADDRESS, mac_trc_lane_swap_regValue);
			WriteTargetWord(dev,MAC_TRC_WFT_CAPTURE_CTRL_ADDRESS, mac_trc_wft[0].regValue);
			//0th bit of MAC_TRC_WFT_STATE_CTRL_ADDRESS should be set to 1 when eventStopIdx is passed else SHOULD BE set to 0
			//WriteTargetWord(dev,MAC_TRC_WFT_STATE_CTRL_ADDRESS, 0xFFFF0001);
			WriteTargetWord(dev,MAC_TRC_WFT_STATE_CTRL_ADDRESS, mac_trc_wft[1].regValue);

			WriteTargetWord(dev,MAC_TRC_TS1_CAPTURE_CTRL_ADDRESS, mac_trc_ts[0][0].regValue);
			WriteTargetWord(dev,MAC_TRC_TS1_STATE_CTRL_ADDRESS, mac_trc_ts[0][1].regValue);
			WriteTargetWord(dev,MAC_TRC_TS1_TRIG_MASK_ADDRESS, mac_trc_ts[0][2].regValue);
			WriteTargetWord(dev,MAC_TRC_TS1_TRIG_ADDRESS, mac_trc_ts[0][3].regValue);

			//WriteTargetWord(dev,MAC_TRC_TS2_CAPTURE_CTRL_ADDRESS, 0xff);
			WriteTargetWord(dev,MAC_TRC_TS2_CAPTURE_CTRL_ADDRESS, mac_trc_ts[1][0].regValue);
			//WriteTargetWord(dev,MAC_TRC_TS2_STATE_CTRL_ADDRESS, 0x30001);
			WriteTargetWord(dev,MAC_TRC_TS2_STATE_CTRL_ADDRESS, mac_trc_ts[1][1].regValue);
			//WriteTargetWord(dev,MAC_TRC_TS2_TRIG_MASK_ADDRESS, 0x01000000);
			WriteTargetWord(dev,MAC_TRC_TS2_TRIG_MASK_ADDRESS, mac_trc_ts[1][2].regValue);
			//WriteTargetWord(dev,MAC_TRC_TS2_TRIG_ADDRESS, 0x0);
			WriteTargetWord(dev,MAC_TRC_TS2_TRIG_ADDRESS, mac_trc_ts[1][3].regValue);

			WriteTargetWord(dev,MAC_TRC_TS3_CAPTURE_CTRL_ADDRESS, mac_trc_ts[2][0].regValue);
			WriteTargetWord(dev,MAC_TRC_TS3_STATE_CTRL_ADDRESS, mac_trc_ts[2][1].regValue);
			WriteTargetWord(dev,MAC_TRC_TS3_TRIG_MASK_ADDRESS, mac_trc_ts[2][2].regValue);
			WriteTargetWord(dev,MAC_TRC_TS3_TRIG_ADDRESS, mac_trc_ts[2][3].regValue);

			WriteTargetWord(dev,MAC_TRC_TS4_CAPTURE_CTRL_ADDRESS, mac_trc_ts[3][0].regValue);
			WriteTargetWord(dev,MAC_TRC_TS4_STATE_CTRL_ADDRESS, mac_trc_ts[3][1].regValue);
			WriteTargetWord(dev,MAC_TRC_TS4_TRIG_MASK_ADDRESS, mac_trc_ts[3][2].regValue);
			WriteTargetWord(dev,MAC_TRC_TS4_TRIG_ADDRESS, mac_trc_ts[3][3].regValue);

			WriteTargetWord(dev,MAC_TRC_TS5_CAPTURE_CTRL_ADDRESS, mac_trc_ts[4][0].regValue);
			WriteTargetWord(dev,MAC_TRC_TS5_STATE_CTRL_ADDRESS, mac_trc_ts[4][1].regValue);
			WriteTargetWord(dev,MAC_TRC_TS5_TRIG_MASK_ADDRESS, mac_trc_ts[4][2].regValue);
			WriteTargetWord(dev,MAC_TRC_TS5_TRIG_ADDRESS, mac_trc_ts[4][3].regValue);

		}else{
			usage();
		}
		break;
	case DIAG_EVENT_CFG:

	    if ((flag & (FILE_FLAG)) ==
                FILE_FLAG)
		{
			if ((dump_fd = fopen(filename, "r")) < 0)
			{
				fprintf(stderr, "err %s cannot create/open output file (%s)\n",
                    __FUNCTION__, filename);
				exit(1);
			}
			//Processing eventCfg file BEGIN
			char buf[1000];
			int eventEnable=0;
			int tokenCount=0;
			int eventNum = 0;
			A_UINT32 wftStateControl = 0xFFFF0000;

			char s[] = " \t";
			char *token;
			//char *eventName;
			char *eventCfg_begin_delim = "#EVENT_CFG_BEGIN";
			char *eventCfg_end_delim = "#EVENT_CFG_END";
			char *eventStop_begin_delim = "#EVENT_STOP_BEGIN";
			char *eventStop_end_delim = "#EVENT_STOP_END";
			char *bufferCfg_begin_delim = "#BUFFER_CFG_BEGIN";
			char *bufferCfg_end_delim = "#BUFFER_CFG_END";
			int eventCfg_flag = 0;
			char *eventStopName = NULL;
			//int RingOrFixedBuffer;
			char line[1000];

			while (fgets(buf,1000, dump_fd)!=NULL)
			{	strcpy(line,buf);
				if(strtok(line," \t\r\n") == NULL){//if a blank line is found, go to the next line \r for linux
				//printf ("Empty line: \n");
				continue;
				}else{
					char comment_delim = '#';
					int index=255;
					char *ptr = strchr(buf,comment_delim);
					char *parameter = NULL;
					A_UINT32 param_value = 0;
					if(ptr){
						index = ptr - buf;
						//printf("\'#\'found: at %d in %s",index,ptr);
					}
					if(index == 0){//if it's a comment line process the comment to see if it's a delimiter
						if(strstr(buf,eventCfg_begin_delim) != NULL){//checking is it's eventCfg begin comment line
						eventCfg_flag = 1;
						continue;
						}
						if(strstr(buf,eventCfg_end_delim) != NULL){//checking is it's eventCfg end comment line
						eventCfg_flag = 0;
						continue;
						}

						if(strstr(buf,eventStop_begin_delim) != NULL){//checking is it's eventCfg begin comment line
						eventCfg_flag |= EVENTSTOP_BEGIN_FLAG;
						continue;
						}
						if(strstr(buf,eventStop_end_delim) != NULL){//checking is it's eventCfg end comment line
						eventCfg_flag &= EVENTSTOP_END_FLAG;
						continue;
						}

						if(strstr(buf,bufferCfg_begin_delim) != NULL){//checking is it's eventCfg begin comment line
						eventCfg_flag |= BUFFER_CFG_BEGIN_FLAG;
						continue;
						}
						if(strstr(buf,bufferCfg_end_delim) != NULL){//checking is it's eventCfg end comment line
						eventCfg_flag &= BUFFER_CFG_END_FLAG;
						continue;
						}
						continue;//if comment is not a delimiter, go to next line
					}
					if((eventCfg_flag & EVENTCFG_BEGIN_FLAG) == EVENTCFG_BEGIN_FLAG){
						tokenCount=0;
						//Remove the trailing new line to help extract event enable
						strtok(buf,"\n");
						token = strtok(buf, s);
						while( token != NULL )
						{
							if(tokenCount == 0){
								eventNum = atoi(token);//event Num 1st argument in the config file
							}else if(tokenCount == 1){
								//eventName = token;//event name---2nd argument in the config file
							}else if(tokenCount == 2){
								eventEnable = atoi(token);//event enable---3rd argument in the config file
							}
							token = strtok(NULL, s);
							tokenCount++;
							//if(*eventName
						}
						eventSelect[eventNum]=eventEnable;
					}
					if((eventCfg_flag & EVENTSTOP_BEGIN_FLAG) == EVENTSTOP_BEGIN_FLAG){
						tokenCount=0;
						//Remove the trailing new line to help extract event enable
						strtok(buf,"\n");
						token = strtok(buf, s);
						//printf ("%s###",buf);
					   while( token != NULL )
						{
							if(tokenCount == 0){
								StopEventIdx = atoi(token);//event Num 1st argument in the config file
							}else if(tokenCount == 1){
								eventStopName = token;//event name ---2nd argument in the config file
							}else if(tokenCount == 2){

								//eventStopData_flag = 1;
								eventStopDataMask = strtol(token,NULL,0);//eventStopDataMask---3rd argument in the config file
							}else if(tokenCount == 3){
								eventCfg_flag |= EVENTSTOPDATA_BEGIN_FLAG;//set eventStopData flag
								//eventStopData_flag = 1;
								eventStopDataValue = strtol(token,NULL,0);//eventStopDataValue---3rd argument in the config file
							}
							//printf ("tokenCount: %d----tokenStr: %s---tokenInt: %d\n",tokenCount,token,atoi(token));
							token = strtok(NULL, s);
							tokenCount++;
						}
						if((eventCfg_flag & EVENTSTOPDATA_BEGIN_FLAG) == EVENTSTOPDATA_BEGIN_FLAG){
							printf("eventStopOnData: EventIdx: %d DataMask: %#x value: %#x:\n",StopEventIdx,eventStopDataMask,eventStopDataValue);
							//eventCfg_flag &= EVENTSTOPDATA_END_FLAG; //assuming only one entry of eventstopdata
						}else{
							printf("eventStop: EventIdx: %d eventName: %s \n",StopEventIdx,eventStopName);
						}
						continue;
					}

					if((eventCfg_flag & BUFFER_CFG_BEGIN_FLAG) == BUFFER_CFG_BEGIN_FLAG){
						tokenCount=0;
						//printf("inside buffercfg if\nbuf: %s",buf);

						//Remove the trailing new line to help extract event enable
						strtok(buf,"\n");
						token = strtok(buf, ": \t");

						 while( token != NULL )
						{
							if(tokenCount == 0){
								parameter = token;//event Num 1st argument in the config file
							}else if(tokenCount == 1){
								//mac_trc_ts1_capture_ctrl_cap_mode = strtol(token,NULL,0);//event name ---2nd argument in the config file
								param_value = strtol(token,NULL,0);//event name ---2nd argument in the config file
							}
							//printf ("tokenCount: %d----tokenStr: %s---tokenInt: %d\n",tokenCount,token,strtol(token,NULL,0));
							token = strtok(NULL, s);
							tokenCount++;
						}
						if(strcmp(parameter, "MAC_TRC_BUF_CFG_VALUE") == 0){//RingOrFixedBuffer
							macTraceBaseRegValue |= (( param_value << MAC_TRC_BUF_CFG_VALUE_LSB) & MAC_TRC_BUF_CFG_VALUE_MASK);
						}else if(strcmp(parameter, "MAC_TRC_ADDR_START_VALUE") == 0){//MacTraceAddrStart
							MacTraceAddrStart =  param_value;
						}else if(strcmp(parameter, "MAC_TRC_ADDR_END_VALUE") == 0){//MacTraceAddrEnd
							MacTraceAddrEnd = param_value;
						}
						//mac_trace_address_value = ((MacTraceAddrStart << MAC_TRC_ADDR_START_VALUE_LSB) & MAC_TRC_ADDR_START_VALUE_MASK) | ((MacTraceAddrEnd << MAC_TRC_ADDR_END_VALUE_LSB) & MAC_TRC_ADDR_END_VALUE_MASK);
						//printf("BufferCfg: %d %#x %#x %#x\n",RingOrFixedBuffer,MacTraceAddrStart,MacTraceAddrEnd,mac_trace_address_value);
						continue;
					}
				}
			}
			mac_trace_address_value = ((MacTraceAddrStart << MAC_TRC_ADDR_START_VALUE_LSB) & MAC_TRC_ADDR_START_VALUE_MASK) | ((MacTraceAddrEnd << MAC_TRC_ADDR_END_VALUE_LSB) & MAC_TRC_ADDR_END_VALUE_MASK);
			//processing eventCfg file END

			int k;
			int eventbus_block_mask_address_value=0;
			int eventStopValue=0;
			for (k=0; k< (sizeof(eventSelect)/sizeof(eventSelect[0]));k++){
				if(eventSelect[k]==1){
					if(k >= 0 && k<= 2){//it's a TOP_LEVEL event
							eventbus_block_mask_address_value |= (1 << TOP_LEVEL_MODULE_ID);
					}
					if(k >= 3 && k<= 39){//it's a SCH event
						eventbus_block_mask_address_value |= (1 << SCH_MODULE_ID);

						if (((flag & FILE_FLAG ) == FILE_FLAG) && k == StopEventIdx){//checking if eventStopIdx arg is passed
							eventStopValue = (SCH_MODULE_ID<<26) | (eventIdInfo[StopEventIdx]<<20) | eventStopDataValue;
							eventStopMask |= eventStopDataMask;
							wftStateControl |= 1;
							printf("SCH: eventStopValue: %#x eventIdInfo: %#x\n",eventStopValue,eventIdInfo[k]);

						}
						if(eventIdInfo[k]<0x20){
							moduleRegInfo[SCH_MODULE_ID][LOW32].regValue |= (1 << eventIdInfo[k]);

						}else{
							moduleRegInfo[SCH_MODULE_ID][HIGH32].regValue |= (1 << (eventIdInfo[k] - 0x20));

						}
					}else if(k >=40 && k <=76){//it's a PDG event
						eventbus_block_mask_address_value |= (1 << PDG_MODULE_ID);
						if (((flag & FILE_FLAG ) == FILE_FLAG) && k == StopEventIdx){//checking if eventStopIdx arg is passed
							eventStopValue = (PDG_MODULE_ID<<26) | (eventIdInfo[StopEventIdx]<<20) | eventStopDataValue;
							eventStopMask |= eventStopDataMask;
							wftStateControl |= 1;
							printf("PDG: eventStopValue: %#x eventIdInfo: %#x\n",eventStopValue,eventIdInfo[k]);
						}
						if(eventIdInfo[k]<0x20){
							moduleRegInfo[PDG_MODULE_ID][LOW32].regValue |= (1 << eventIdInfo[k]);
						}else{
							moduleRegInfo[PDG_MODULE_ID][HIGH32].regValue |= (1 << (eventIdInfo[k] - 0x20));
						}
					}else if(k >=77 && k <=102){//it's a TXDMA event
						eventbus_block_mask_address_value |= (1 << TXDMA_MODULE_ID);
						if (((flag & FILE_FLAG ) == FILE_FLAG) && k == StopEventIdx){//checking if eventStopIdx arg is passed
							eventStopValue = (TXDMA_MODULE_ID<<26) | (eventIdInfo[StopEventIdx]<<20) | eventStopDataValue;
							eventStopMask |= eventStopDataMask;
							wftStateControl |= 1;
							printf("TXDMA: eventStopValue: %#x eventIdInfo: %#x\n",eventStopValue,eventIdInfo[k]);
						}
						if(eventIdInfo[k]<0x20){
							moduleRegInfo[TXDMA_MODULE_ID][LOW32].regValue |= (1 << eventIdInfo[k]);
						}else{
							moduleRegInfo[TXDMA_MODULE_ID][HIGH32].regValue |= (1 << (eventIdInfo[k] - 0x20));
						}
					}else if(k >= 103 && k<= 121){//it's a RXDMA event
						eventbus_block_mask_address_value |= (1 << RXDMA_MODULE_ID);
						if (((flag & FILE_FLAG ) == FILE_FLAG) && k == StopEventIdx){//checking if eventStopIdx arg is passed
							eventStopValue = (RXDMA_MODULE_ID<<26) | (eventIdInfo[StopEventIdx]<<20) | eventStopDataValue;
							eventStopMask |= eventStopDataMask;
							wftStateControl |= 1;
						}
						if(eventIdInfo[k]<0x20){
							moduleRegInfo[RXDMA_MODULE_ID][LOW32].regValue |= (1 << eventIdInfo[k]);
							moduleRegInfo[RXDMA_MODULE_ID][HIGH32].regValue |= (1 << eventIdInfo[k]);//since RXDMA doesn't have HIGH32 masks
						}else{
							moduleRegInfo[RXDMA_MODULE_ID][HIGH32].regValue |= (1 << (eventIdInfo[k] - 0x20));
						}
					}else if(k >= 122 && k<= 173){//it's an OLE event
						eventbus_block_mask_address_value |= (1 << OLE_MODULE_ID);
						if (((flag & FILE_FLAG ) == FILE_FLAG) && k == StopEventIdx){//checking if eventStopIdx arg is passed
							eventStopValue = (OLE_MODULE_ID<<26) | (eventIdInfo[StopEventIdx]<<20) | eventStopDataValue;
							eventStopMask |= eventStopDataMask;
							wftStateControl |= 1;
							printf("OLE: eventStopValue: %#x eventIdInfo: %#x\n",eventStopValue,eventIdInfo[k]);
						}
						if(eventIdInfo[k]<0x20){
							moduleRegInfo[OLE_MODULE_ID][LOW32].regValue |= (1 << eventIdInfo[k]);
						}else{
							moduleRegInfo[OLE_MODULE_ID][HIGH32].regValue |= (1 << (eventIdInfo[k] - 0x20));
						}
					}else if(k >= 174 && k<= 198){//it's a CRYPTO event
						eventbus_block_mask_address_value |= (1 << CRYPTO_MODULE_ID);
						if (((flag & FILE_FLAG ) == FILE_FLAG) && k == StopEventIdx){//checking if eventStopIdx arg is passed
						eventStopValue = (CRYPTO_MODULE_ID<<26) | (eventIdInfo[StopEventIdx]<<20) | eventStopDataValue;
						eventStopMask |= eventStopDataMask;
						wftStateControl |= 1;
						printf("CRYPTO: eventStopValue: %#x eventIdInfo: %#x\n",eventStopValue,eventIdInfo[k]);
						}
						if(eventIdInfo[k]<0x20){
							moduleRegInfo[CRYPTO_MODULE_ID][LOW32].regValue |= (1 << eventIdInfo[k]);
							moduleRegInfo[CRYPTO_MODULE_ID][HIGH32].regValue |= (1 << eventIdInfo[k]);//since CRYPTO doesn't have HIGH32 masks
						}else{
							moduleRegInfo[CRYPTO_MODULE_ID][HIGH32].regValue |= (1 << (eventIdInfo[k] - 0x20));
						}
					}else if(k >= 199 && k<= 248){//it's a TXPCU event
						eventbus_block_mask_address_value |= (1 << TXPCU_MODULE_ID);
						if (((flag & FILE_FLAG ) == FILE_FLAG) && k == StopEventIdx){//checking if eventStopIdx arg is passed
						eventStopValue = (TXPCU_MODULE_ID<<26) | (eventIdInfo[StopEventIdx]<<20) | eventStopDataValue;
						eventStopMask |= eventStopDataMask;
						wftStateControl |= 1;
						printf("TXPCU: eventStopValue: %#x eventIdInfo: %#x\n",eventStopValue,eventIdInfo[k]);
						}
						if(eventIdInfo[k]<0x20){
							moduleRegInfo[TXPCU_MODULE_ID][LOW32].regValue |= (1 << eventIdInfo[k]);
						}else{
							moduleRegInfo[TXPCU_MODULE_ID][HIGH32].regValue |= (1 << (eventIdInfo[k] - 0x20));
						}
					}else if(k >= 249 && k<= 299){//it's a RXPCU event
						eventbus_block_mask_address_value |= (1 << RXPCU_MODULE_ID);
						if (((flag & FILE_FLAG ) == FILE_FLAG) && k == StopEventIdx){//checking if eventStopIdx arg is passed
						eventStopValue = (RXPCU_MODULE_ID<<26) | (eventIdInfo[StopEventIdx]<<20) | eventStopDataValue;
						eventStopMask |= eventStopDataMask;
						wftStateControl |= 1;
						printf("RXPCU: eventStopValue: %#x eventIdInfo: %#x\n",eventStopValue,eventIdInfo[k]);
						}
						if(eventIdInfo[k]<0x20){
							moduleRegInfo[RXPCU_MODULE_ID][LOW32].regValue |= (1 << eventIdInfo[k]);
						}else{
							moduleRegInfo[RXPCU_MODULE_ID][HIGH32].regValue |= (1 << (eventIdInfo[k] - 0x20));
						}
					}else if(k >= 300 && k<= 301){//it's a SW event
						eventbus_block_mask_address_value |= (1 << SW_MODULE_ID);
					}
				}
			}

			fclose(dump_fd);

			WriteTargetWord(dev,MCMN_EVENT_TRACE_BUS_SELECT_ADDRESS, 0x1);
			//write this register to enable module level events
			printf("eventbus_block_mask_address_value: %#x\n",eventbus_block_mask_address_value);
			WriteTargetWord(dev,MCMN_EVENTBUS_BLOCK_MASK_ADDRESS, eventbus_block_mask_address_value);
			//WriteTargetWord(dev,MCMN_EVENTBUS_BLOCK_MASK_ADDRESS, 0x08);//TxDMA only
			//to enable timestamp
			WriteTargetWord(dev,MAC_TRC_TIME_STAMP_ADDRESS, 0x1);

			//bit 1 if set to 1---wrap around, else it's single shot
			WriteTargetWord(dev,__MAC_TRC_REG_BASE_ADDRESS, macTraceBaseRegValue);

			//MAC_TRC_ADDR---0:13---start addr, 16:29---end addr
			//mac_trace_address_value = ((MacTraceAddrStart << MAC_TRC_ADDR_START_VALUE_LSB) & MAC_TRC_ADDR_START_VALUE_MASK) | ((MacTraceAddrEnd << MAC_TRC_ADDR_END_VALUE_LSB) & MAC_TRC_ADDR_END_VALUE_MASK);
			printf("%#x mac_trace_address_value: %#x\n",MAC_TRC_ADDR_ADDRESS,mac_trace_address_value);
			WriteTargetWord(dev,MAC_TRC_ADDR_ADDRESS, mac_trace_address_value);
			//MAC_TRC_LANE_SWAP
			WriteTargetWord(dev,MAC_TRC_LANE_SWAP_ADDRESS, 0xF);
			WriteTargetWord(dev,MAC_TRC_WFT_CAPTURE_CTRL_ADDRESS, 0x4fffffff);
			//0th bit of MAC_TRC_WFT_STATE_CTRL_ADDRESS should be set to 1 when eventStopIdx is passed else SHOULD BE set to 0
			//WriteTargetWord(dev,MAC_TRC_WFT_STATE_CTRL_ADDRESS, 0xFFFF0001);
			WriteTargetWord(dev,MAC_TRC_WFT_STATE_CTRL_ADDRESS, wftStateControl);

			WriteTargetWord(dev,MAC_TRC_TS1_CAPTURE_CTRL_ADDRESS, 0x4fffffff);
			WriteTargetWord(dev,MAC_TRC_TS1_STATE_CTRL_ADDRESS, 0x10000);

			WriteTargetWord(dev,MAC_TRC_TS1_TRIG_MASK_ADDRESS, eventStopMask);



			WriteTargetWord(dev,MAC_TRC_TS1_TRIG_ADDRESS, eventStopValue);

			//WriteTargetWord(dev,MAC_TRC_TS2_CAPTURE_CTRL_ADDRESS, 0xff);
			WriteTargetWord(dev,MAC_TRC_TS2_CAPTURE_CTRL_ADDRESS, 0xfffffff);
			//WriteTargetWord(dev,MAC_TRC_TS2_STATE_CTRL_ADDRESS, 0x30001);
			WriteTargetWord(dev,MAC_TRC_TS2_STATE_CTRL_ADDRESS, 0xffff0000);
			//WriteTargetWord(dev,MAC_TRC_TS2_TRIG_MASK_ADDRESS, 0x01000000);
			WriteTargetWord(dev,MAC_TRC_TS2_TRIG_MASK_ADDRESS, 0x00000000);
			//WriteTargetWord(dev,MAC_TRC_TS2_TRIG_ADDRESS, 0x0);
			WriteTargetWord(dev,MAC_TRC_TS2_TRIG_ADDRESS, 0x0);
			WriteTargetWord(dev,MAC_TRC_TS3_CAPTURE_CTRL_ADDRESS, 0x0);
			WriteTargetWord(dev,MAC_TRC_TS3_STATE_CTRL_ADDRESS, 0xffff0001);
			WriteTargetWord(dev,MAC_TRC_TS3_TRIG_MASK_ADDRESS, 0x0ffff003);
			WriteTargetWord(dev,MAC_TRC_TS3_TRIG_ADDRESS, 0x0ac22003);

			//register writes for specific event capture
			int moduleId,lowOrHigh32;
			for (moduleId=0;moduleId<9;moduleId++){//need to iterate up to 10 after SW module info is entered
				for (lowOrHigh32=0;lowOrHigh32<=1;lowOrHigh32++){
					//printf("j: %d k: %d regAddr: %#x regValue: %#x\n",moduleId,lowOrHigh32,moduleRegInfo[moduleId][lowOrHigh32].regAddr,moduleRegInfo[moduleId][lowOrHigh32].regValue);
					WriteTargetWord(dev,moduleRegInfo[moduleId][lowOrHigh32].regAddr,moduleRegInfo[moduleId][lowOrHigh32].regValue);
				}
			}
		}else{
			printf("Something wrong with file as argument\n");
            usage();
        }

		break;

	case DIAG_TRACE_START:
		ReadTargetWord(dev, __MAC_TRC_REG_BASE_ADDRESS, &macTraceBaseRegValue);
		macTraceBaseRegValue |= 0x1;//setting bit-0 to 1 to enable tracer
		WriteTargetWord(dev,__MAC_TRC_REG_BASE_ADDRESS, macTraceBaseRegValue);
		break;
	case DIAG_TRACE_STOP:
		ReadTargetWord(dev,__MAC_TRC_REG_BASE_ADDRESS, &macTraceBaseRegValue);
		macTraceBaseRegValue &= ~(1 << MAC_TRC_ENABLE_VALUE_LSB);//clear bit-0 to stop eventcapture
		WriteTargetWord(dev,__MAC_TRC_REG_BASE_ADDRESS, macTraceBaseRegValue);

		//ReadTargetWord(dev, MAC_TRC_STATUS_1_ADDRESS, &bufferReadAddr);
		//WriteTargetWord(dev,__MAC_TRC_REG_BASE_ADDRESS, 0x10);
		break;

    case DIAG_READ_TARGET:
        if ((flag & (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG )) ==
                (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG))
        {
            if ((dump_fd = fopen(filename, "wb+")) == NULL)
            {
                fprintf(stderr, "err %s cannot create/open output file (%s)\n",
                        __FUNCTION__, filename);
                exit(1);
            }

            buffer = (A_UINT8 *)MALLOC(MAX_BUF);
            if (buffer == NULL)
            {
                fprintf(stderr, "err %s malloc failed \n",
                        __FUNCTION__);
                exit(1);
            }

            nqprintf(
                    "DIAG Read Target (address: 0x%x, length: %d, filename: %s)\n",
                    address, length, filename);


            {
                unsigned int remaining = length;

				unsigned int i = 0;
				unsigned int cc;

                if(flag & HEX_FLAG)
                {
                    if (flag & OTP_FLAG) {
                        fprintf(dump_fd,"target otp dump area[0x%08x - 0x%08x]",address,address+length);
                    } else {
                        fprintf(dump_fd,"target mem dump area[0x%08x - 0x%08x]",address,address+length);
                    }
                }
                while (remaining)
                {
                    length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
                    if (flag & OTP_FLAG) {
                        ReadTargetOTP(dev, address, buffer, length);
                    } else {
                        ReadTargetRange(dev, address, buffer, length);
                    }

                    if(flag & HEX_FLAG)
                    {
                        for(i=0;i<length;i+=4)
                        {
                            if(i%16 == 0)
                                fprintf(dump_fd,"\n0x%08x:\t",address+i);
                            fprintf(dump_fd,"0x%08lx\t",(long unsigned int)*(A_UINT32*)(buffer+i));
                        }
                    }
                    else
                    {
                        fwrite(buffer,1 , length, dump_fd);
                    }

                    /*  swapping the values as the HIF layer swaps the bytes on a 4-byte boundary
                        which causes reverse in the bytes, example if a[4], then a[3] is read as
                        a[0] only u32 remain same*/

                   if (CAL_FLAG) {
                        if ((pFile = fopen("Driver_Cal_structure.txt", "w")) == NULL)
                        {
                             fprintf(stderr, "err %s cannot create/open output file Driver_Cal_structure.txt\n",
                                 __FUNCTION__);
                             exit(1);
                        }
                        for(i = 0; i < length; i = i + 4) {
                            cc = buffer[i];
                            buffer[i] = buffer[i+3];
                            buffer[i+3] = cc;
                            cc = buffer[i+1];
                            buffer[i+1] = buffer[i+2];
                            buffer[i+2] = cc;
                    	}
	                    pQc9kEepromArea = buffer;
    	                PrintQc98xxStruct((QC98XX_EEPROM *)buffer, 0);
        	            fclose(pFile);
                    }

					remaining -= length;
                    address += length;
                }
            }
            fclose(dump_fd);
            free(buffer);
        } else {
            usage();
        }
        break;

    case DIAG_WRITE_TARGET:
        if (!(flag & ADDRESS_FLAG))
        {
            usage(); /* no address specified */
        }
        if (!(flag & (FILE_FLAG | PARAM_FLAG)))
        {
            usage(); /* no data specified */
        }
        if ((flag & FILE_FLAG) && (flag & PARAM_FLAG))
        {
            usage(); /* too much data specified */
        }

        if (flag & FILE_FLAG)
        {
            struct stat filestat;
            unsigned int file_length;

            if ((fd = open(filename, O_RDONLY)) < 0)
            {
                fprintf(stderr, "err %s Could not open file (%s)\n", __FUNCTION__, filename);
                exit(1);
            }
            memset(&filestat, '\0', sizeof(struct stat));
            buffer = (A_UINT8 *)MALLOC(MAX_BUF);
            fstat(fd, &filestat);
            file_length = filestat.st_size;
            if (file_length == 0) {
                fprintf(stderr, "err %s Zero length input file (%s)\n", __FUNCTION__, filename);
                exit(1);
            }

            if (flag & LENGTH_FLAG) {
                if (length > file_length) {
                    fprintf(stderr, "err %s file %s: length (%d) too short (%d)\n", __FUNCTION__,
                        filename, file_length, length);
                    exit(1);
                }
            } else {
                length = file_length;
            }

            nqprintf(
                 "DIAG Write Target (address: 0x%x, filename: %s, length: %d)\n",
                  address, filename, length);

        }
        else
        { /* PARAM_FLAG */
            nqprintf(
                 "DIAG Write Word (address: 0x%lx, value: 0x%lx)\n",
                 (long unsigned int)address, (long unsigned int)param);
            length = sizeof(param);
            buffer = (A_UINT8 *)&param;
            fd = -1;
        }

        /*
         * Write length bytes of data to memory/OTP.
         * Data is either present in buffer OR
         * needs to be read from fd in MAX_BUF chunks.
         *
         * Within the kernel, the implementation of
         * DIAG_WRITE_TARGET further limits the size
         * of each transfer over the interconnect.
         */
        {
            unsigned int remaining = 0;
            unsigned int otp_check_address = address;

            if (flag & OTP_FLAG) { /* Validate OTP write before committing anything */
                remaining = length;
                while (remaining)
                {
                    int nbyte;

                    length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
                    if (fd > 0)
                    {
                        nbyte = read(fd, buffer, length);
                        if (nbyte != length) {
                            fprintf(stderr, "err %s read from file failed (%d)\n", __FUNCTION__, nbyte);
                            exit(1);
                        }
                    }

                    if ((flag & OTP_FLAG) && !ValidWriteOTP(dev, otp_check_address, buffer, length))
                    {
                            exit(1);
                    }

                    remaining -= length;
                    otp_check_address += length;
                }
                (void)lseek(fd, 0, SEEK_SET);
            }

            remaining = length;
            while (remaining)
            {
                int nbyte;

                length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
                if (fd > 0)
                {
                    nbyte = read(fd, buffer, length);
                    if (nbyte != length) {
                        fprintf(stderr, "err %s read from file failed (%d)\n", __FUNCTION__, nbyte);
                        exit(1);
                    }
                }

                if (flag & OTP_FLAG) {
                    WriteTargetOTP(dev, address, buffer, length);
                } else {
                    WriteTargetRange(dev, address, buffer, length);
                }

                remaining -= length;
                address += length;
            }
        }

        if (flag & FILE_FLAG) {
            free(buffer);
            close(fd);
        }

        break;

    case DIAG_READ_WORD:
        if ((flag & (ADDRESS_FLAG)) == (ADDRESS_FLAG))
	{

		for ( ; wordcount; address += 4, wordcount--) {
			nqprintf("DIAG Read Word (address: 0x%x)\n", address);
			ReadTargetWord(dev, address, &param);

			if (quiet()) {
				printf("0x%08x\t", param);
				if ((wordcount % 4) == 1) printf("\n");
			} else {
				printf("Value in target at 0x%x: 0x%x (%d)\n", address, param, param);
			}
		}
	}
        else usage();
        break;

    case DIAG_WRITE_WORD:
        if ((flag & (ADDRESS_FLAG | PARAM_FLAG)) == (ADDRESS_FLAG | PARAM_FLAG))
        {
            A_UINT32 origvalue = 0;

            if (flag & BITWISE_OP_FLAG) {
                /* first read */
                ReadTargetWord(dev, address, &origvalue);
                param = origvalue;

                /* now modify */
                if (flag & AND_OP_FLAG) {
                    param &= bitwise_mask;
                } else {
                    param |= bitwise_mask;
                }

                /* fall through to write out the parameter */
            }

            if (flag & BITWISE_OP_FLAG) {
                if (quiet()) {
                    printf("0x%lx\n", (long unsigned int)origvalue);
                } else {
                    printf("DIAG Bit-Wise (%s) modify Word (address: 0x%lx, orig:0x%lx, new: 0x%lx,  mask:0x%lX)\n",
                       (flag & AND_OP_FLAG) ? "AND" : "OR", (long unsigned int)address, (long unsigned int)origvalue, (long unsigned int)param, (long unsigned int)bitwise_mask );
                }
            } else{
                nqprintf("DIAG Write Word (address: 0x%lx, param: 0x%lx)\n", (long unsigned int)address, (long unsigned int)param);
            }

            WriteTargetWord(dev, address, param);
        }
        else usage();
        break;

    case DIAG_PHYTLV_CNT:
    {
      int i=0;


      for(i=0; i < 2; i++) {

            /* sudo athdiag --get --address=0x30890 */
            address = 0x30890;
            ReadTargetWord(dev, address, &param);
            printf("status dma hw/fw idx           0x%04x 0x%04x\n", (param >> 16) & 0xffff, param & 0xffff);
            address = 0x30894;
            ReadTargetWord(dev, address, &param);
            printf("local dma  hw/fw idx           0x%04x 0x%04x\n", (param >> 16) & 0xffff, param & 0xffff);
            address = 0x30898;
            ReadTargetWord(dev, address, &param);
            printf("remote dma hw/fw idx           0x%04x 0x%04x\n", (param >> 16) & 0xffff, param & 0xffff);



            //print_cnt(dev, address);

            /* sudo athdiag --get --address=0x32174 */
            address = 0x32174;
            ReadTargetWord(dev, address, &param);
            printf("ppdu end/rssi legacy           0x%04x 0x%04x\n", (param >> 16) & 0xffff, param & 0xffff);
            //print_cnt(dev, address);
            /* sudo athdiag --get --address=0x32178 */
            address = 0x32178;
            ReadTargetWord(dev, address, &param);
            printf("11b/pkt end/pkt user/rx term   0x%02x 0x%02x 0x%02x 0x%02x\n", (param >> 24) & 0xff, (param >> 16) & 0xff, (param >> 8) & 0xff, param & 0xff);
            //print_cnt(dev, address);
            /* sudo athdiag --get --address=0x3217c */
            address = 0x3217c;
            ReadTargetWord(dev, address, &param);
            printf("11ac-mu/11ac-su/11n/11a        0x%02x 0x%02x 0x%02x 0x%02x\n", (param >> 24) & 0xff, (param >> 16) & 0xff, (param >> 8) & 0xff, param & 0xff);
            //print_cnt(dev, address);

            sleep(1);
            printf("\n");
        }
    }

        break;
    case DIAG_PHYERR_CNT:
        {
            int i=0;

            nqprintf("setting phy_err_mask\n");

            /*phy_err_1 to 0x0 */
            address = 0x3213c;
            param = 0x0;
            WriteTargetWord(dev, address, param);

            /*phy_err_2 to 0x0 */
            address = 0x32148;
            param = 0x0;
            WriteTargetWord(dev, address, param);


            /*phy_err_3 to 0x0 */
            address = 0x32154;
            param = 0x0;
            WriteTargetWord(dev, address, param);



            /* athdiag --set --address=0x32140 --value=0x20 > tmp.txt */
            address = 0x32140;
            param = 0x20;
            WriteTargetWord(dev, address, param);

            /* athdiag --set --address=0x32144 --value=0x0 > tmp.txt */
            address = 0x32144;
            param = 0x0;
            WriteTargetWord(dev, address, param);

            /* athdiag --set --address=0x3214c --value=0x2000 > tmp.txt */
            address = 0x3214c;
            param = 0x2000;
            WriteTargetWord(dev, address, param);

            /* athdiag --set --address=0x32150 --value=0x0 > tmp.txt */
            address = 205136;
            param = 0;
            WriteTargetWord(dev, address, param);

            /* athdiag --set --address=0x32158 --value=0xffffffff */
            address = 0x32158;
            param = 0xffffffff;
            WriteTargetWord(dev, address, param);

            /* athdiag --set --address=0x3215c --value=0xffffffff */
            address = 0x3215c;
            WriteTargetWord(dev, address, param);


            for(i=0; i < 2; i++) {

                /* sudo athdiag --get --address=0x3213c */
                address = 0x3213c;
                ReadTargetWord(dev, address, &param);
                printf("phy_err_1  %14d\n", param);

                /* sudo athdiag --get --address=0x32140
                 * sudo athdiag --get --address=0x32144 */
                address = 0x32140;
                ReadTargetWord(dev, address, &param);
                printf("phy_err_1_mask   %08x\n", param);

                address = 0x32144;
                ReadTargetWord(dev, address, &param);
                printf("phy_err_1_maskc  %08x\n", param);

                /* sudo athdiag --get --address=0x32148 */
                address = 0x32148;
                ReadTargetWord(dev, address, &param);
                printf("phy_err_2  %14d\n", param);

                /* sudo athdiag --get --address=0x3214c
                 * sudo athdiag --get --address=0x32150 */
                address = 0x3214c;
                ReadTargetWord(dev, address, &param);
                printf("phy_err_2_mask   %08x\n", param);

                address = 0x32150;
                ReadTargetWord(dev, address, &param);
                printf("phy_err_2_maskc  %08x\n", param);


                /* sudo athdiag --get --address=0x32154 */
                address = 0x32154;
                ReadTargetWord(dev, address, &param);
                printf("phy_err_3  %14d\n", param);


                /* sudo athdiag --get --address=0x32158
                 * sudo athdiag --get --address=0x3215c
                 */
                address = 0x32158;
                ReadTargetWord(dev, address, &param);
                printf("phy_err_3_mask   %08x\n", param);

                address = 0x3215c;
                ReadTargetWord(dev, address, &param);
                printf("phy_err_3_maskc  %08x\n", param);

                sleep(1);
                printf("\n");
            }

        }
        break;
    case DIAG_CYCLE_CNT:
        {
            //TODO
            nqprintf("resetting mib\n");

            /* athdiag --set --address=0x31208 --value=0x4 */
            address = 201224;
            param = 4;
            WriteTargetWord(dev, address, param);

            /* athdiag --set --address=0x31208 --value=0x0 */
            param = 0;
            WriteTargetWord(dev, address, param);

            sleep(1);

            for(i=0;i<1;i++) {
                 /* athdiag --get --address=0x31238 > tmp.txt */
                 address = 0x31238;
                 ReadTargetWord(dev, address, &param);
                 printf("rx_frame  %15d\n", param);

                 /* athdiag --get --address=0x3123c */
                 address = 0x3123c;
                 ReadTargetWord(dev, address, &param);
                 printf("tx_frame  %15d\n", param);

                 /* athdiag --get --address=0x31240 */
                 address = 0x31240;
                 ReadTargetWord(dev, address, &param);
                 printf("rx_busy   %15d\n", param);

                 /* athdiag --get --address=0x31204 */
                 address = 0x31204;
                 ReadTargetWord(dev, address, &param);
                 printf("pcu cycle %15d\n", param);

                 printf("\n");

            }


        }
        break;
    default:
        usage();
    }

    exit (0);
}
