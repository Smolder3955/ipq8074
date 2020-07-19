/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
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

#include "diag.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_eeprom.h"

#include <getopt.h>
#include <errno.h>
#include <err.h>

struct	ath_diag atd;
int	s;
const char *progname;

/* TODO: should be shared */
#define AR5416_BCHAN_UNUSED          0xFF

#ifndef __packed
#define __packed    __attribute__((__packed__))
#endif

#define AR5416_OPFLAGS_11A           0x01   /* if set, allow 11a */
#define AR5416_OPFLAGS_11G           0x02   /* if set, allow 11g */
#define AR5416_OPFLAGS_N_5G_HT40     0x04   /* if set, disable 5G HT40 */
#define AR5416_OPFLAGS_N_2G_HT40     0x08   /* if set, disable 2G HT40 */
#define AR5416_OPFLAGS_N_5G_HT20     0x10   /* if set, disable 5G HT20 */
#define AR5416_OPFLAGS_N_2G_HT20     0x20   /* if set, disable 2G HT20 */

/* RF silent fields in EEPROM */
#define EEP_RFSILENT_ENABLED        1
#define EEP_RFSILENT_POLARITY       0x0002
#define EEP_RFSILENT_POLARITY_S     1
#define EEP_RFSILENT_GPIO_SEL       0x001c
#define EEP_RFSILENT_GPIO_SEL_S     2

#define AR5416_EEP_NO_BACK_VER       0x1
#define AR5416_EEP_VER               0xE
#define AR5416_EEP_VER_MINOR_MASK    0x0FFF
#define AR5416_EEP_MINOR_VER_2       0x2
#define AR5416_EEP_MINOR_VER_3       0x3
#define AR5416_EEP_MINOR_VER_7       0x7

// 16-bit offset location start of calibration struct
#define AR5416_EEP_START_LOC         256
#define AR5416_NUM_5G_CAL_PIERS      8
#define AR5416_NUM_2G_CAL_PIERS      4
#define AR5416_NUM_5G_20_TARGET_POWERS  8
#define AR5416_NUM_5G_40_TARGET_POWERS  8
#define AR5416_NUM_2G_CCK_TARGET_POWERS 3
#define AR5416_NUM_2G_20_TARGET_POWERS  4
#define AR5416_NUM_2G_40_TARGET_POWERS  4
#define AR5416_NUM_CTLS              24
#define AR5416_NUM_BAND_EDGES        8
#define AR5416_NUM_PD_GAINS          4
#define AR5416_PD_GAINS_IN_MASK      4
#define AR5416_PD_GAIN_ICEPTS        5
#define AR5416_EEPROM_MODAL_SPURS    5
#define AR5416_MAX_RATE_POWER        63
#define AR5416_NUM_PDADC_VALUES      128
#define AR5416_NUM_RATES             16
#define AR5416_BCHAN_UNUSED          0xFF
#define AR5416_MAX_PWR_RANGE_IN_HALF_DB 64
#define AR5416_EEPMISC_BIG_ENDIAN    0x01
#define FREQ2FBIN(x,y) ((y) ? ((x) - 2300) : (((x) - 4800) / 5))
#define AR5416_MAX_CHAINS            3
#define AR5416_ANT_16S               25

#define AR5416_NUM_ANT_CHAIN_FIELDS     7
#define AR5416_NUM_ANT_COMMON_FIELDS    4
#define AR5416_SIZE_ANT_CHAIN_FIELD     3
#define AR5416_SIZE_ANT_COMMON_FIELD    4
#define AR5416_ANT_CHAIN_MASK           0x7
#define AR5416_ANT_COMMON_MASK          0xf
#define AR5416_CHAIN_0_IDX              0
#define AR5416_CHAIN_1_IDX              1
#define AR5416_CHAIN_2_IDX              2

#define AR5416_LEGACY_CHAINMASK		1

typedef enum Ar5416_Rates {
    rate6mb,  rate9mb,  rate12mb, rate18mb,
    rate24mb, rate36mb, rate48mb, rate54mb,
    rate1l,   rate2l,   rate2s,   rate5_5l,
    rate5_5s, rate11l,  rate11s,  rateXr,
    rateHt20_0, rateHt20_1, rateHt20_2, rateHt20_3,
    rateHt20_4, rateHt20_5, rateHt20_6, rateHt20_7,
    rateHt40_0, rateHt40_1, rateHt40_2, rateHt40_3,
    rateHt40_4, rateHt40_5, rateHt40_6, rateHt40_7,
    rateDupCck, rateDupOfdm, rateExtCck, rateExtOfdm,
    Ar5416RateSize
} AR5416_RATES;

typedef struct BaseEepHeader {
    u_int16_t  length;
    u_int16_t  checksum;
    u_int16_t  version;
    u_int8_t   opCapFlags;
    u_int8_t   eepMisc;
    u_int16_t  regDmn[2];
    u_int8_t   macAddr[6];
    u_int8_t   rxMask;
    u_int8_t   txMask;
    u_int16_t  rfSilent;
    u_int16_t  blueToothOptions;
    u_int16_t  deviceCap;
    u_int32_t  binBuildNumber;
    u_int8_t   deviceType;
    u_int8_t   futureBase[33];
} __packed BASE_EEP_HEADER; // 64 B

typedef struct spurChanStruct {
    u_int16_t spurChan;
    u_int8_t  spurRangeLow;
    u_int8_t  spurRangeHigh;
} __packed SPUR_CHAN;

typedef struct ModalEepHeader {
    u_int32_t  antCtrlChain[AR5416_MAX_CHAINS];       // 12
    u_int32_t  antCtrlCommon;                         // 4
    u_int8_t   antennaGainCh[AR5416_MAX_CHAINS];      // 3
    u_int8_t   switchSettling;                        // 1
    u_int8_t   txRxAttenCh[AR5416_MAX_CHAINS];        // 3
    u_int8_t   rxTxMarginCh[AR5416_MAX_CHAINS];       // 3
    u_int8_t    adcDesiredSize;                        // 1
    u_int8_t    pgaDesiredSize;                        // 1
    u_int8_t   xlnaGainCh[AR5416_MAX_CHAINS];         // 3
    u_int8_t   txEndToXpaOff;                         // 1
    u_int8_t   txEndToRxOn;                           // 1
    u_int8_t   txFrameToXpaOn;                        // 1
    u_int8_t   thresh62;                              // 1
    u_int8_t    noiseFloorThreshCh[AR5416_MAX_CHAINS]; // 3
    u_int8_t   xpdGain;                               // 1
    u_int8_t   xpd;                                   // 1
    u_int8_t    iqCalICh[AR5416_MAX_CHAINS];           // 1
    u_int8_t    iqCalQCh[AR5416_MAX_CHAINS];           // 1
    u_int8_t   pdGainOverlap;                         // 1
    u_int8_t   ob;                                    // 1
    u_int8_t   db;                                    // 1
    u_int8_t   xpaBiasLvl;                            // 1
    u_int8_t   pwrDecreaseFor2Chain;                  // 1
    u_int8_t   pwrDecreaseFor3Chain;                  // 1 -> 48 B
    u_int8_t   txFrameToDataStart;                    // 1
    u_int8_t   txFrameToPaOn;                         // 1
    u_int8_t   ht40PowerIncForPdadc;                  // 1
    u_int8_t   bswAtten[AR5416_MAX_CHAINS];           // 3
    u_int8_t   bswMargin[AR5416_MAX_CHAINS];          // 3
    u_int8_t   swSettleHt40;                          // 1
    u_int8_t   futureModalMerlin[10];                 // 10
    u_int16_t  xpaBiasLvlFreq[3];                     // 3
    u_int8_t   futureModal[6];                        // 6
    SPUR_CHAN spurChans[AR5416_EEPROM_MODAL_SPURS];  // 20 B
} __packed MODAL_EEP_HEADER;                    // == 100 B

typedef struct calDataPerFreq {
    u_int8_t pwrPdg[AR5416_NUM_PD_GAINS][AR5416_PD_GAIN_ICEPTS];
    u_int8_t vpdPdg[AR5416_NUM_PD_GAINS][AR5416_PD_GAIN_ICEPTS];
} __packed CAL_DATA_PER_FREQ;

typedef struct CalTargetPowerLegacy {
    u_int8_t  bChannel;
    u_int8_t  tPow2x[4];
} __packed CAL_TARGET_POWER_LEG;

typedef struct CalTargetPowerHt {
    u_int8_t  bChannel;
    u_int8_t  tPow2x[8];
} __packed CAL_TARGET_POWER_HT;

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
typedef struct CalCtlEdges {
    u_int8_t  bChannel;
    u_int8_t  flag   :2,
             tPower :6;
} __packed CAL_CTL_EDGES;
#else
typedef struct CalCtlEdges {
    u_int8_t  bChannel;
    u_int8_t  tPower :6,
             flag   :2;
} __packed CAL_CTL_EDGES;
#endif

typedef struct CalCtlData {
    CAL_CTL_EDGES  ctlEdges[AR5416_MAX_CHAINS][AR5416_NUM_BAND_EDGES];
} __packed CAL_CTL_DATA;

typedef struct eepMap {
    BASE_EEP_HEADER      baseEepHeader;         // 64 B
    u_int8_t             custData[64];                   // 64 B
    MODAL_EEP_HEADER     modalHeader[2];        // 200 B
    u_int8_t             calFreqPier5G[AR5416_NUM_5G_CAL_PIERS];
    u_int8_t             calFreqPier2G[AR5416_NUM_2G_CAL_PIERS];
    CAL_DATA_PER_FREQ    calPierData5G[AR5416_MAX_CHAINS][AR5416_NUM_5G_CAL_PIERS];
    CAL_DATA_PER_FREQ    calPierData2G[AR5416_MAX_CHAINS][AR5416_NUM_2G_CAL_PIERS];
    CAL_TARGET_POWER_LEG calTargetPower5G[AR5416_NUM_5G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower5GHT20[AR5416_NUM_5G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower5GHT40[AR5416_NUM_5G_40_TARGET_POWERS];
    CAL_TARGET_POWER_LEG calTargetPowerCck[AR5416_NUM_2G_CCK_TARGET_POWERS];
    CAL_TARGET_POWER_LEG calTargetPower2G[AR5416_NUM_2G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower2GHT20[AR5416_NUM_2G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower2GHT40[AR5416_NUM_2G_40_TARGET_POWERS];
    u_int8_t             ctlIndex[AR5416_NUM_CTLS];
    CAL_CTL_DATA         ctlData[AR5416_NUM_CTLS];
    u_int8_t             padding;
} __packed ar5416_eeprom_t;  // EEP_MAP


static inline u_int16_t
fbin2freq(u_int8_t fbin, HAL_BOOL is2GHz)
{
    /*
     * Reserved value 0xFF provides an empty definition both as
     * an fbin and as a frequency - do not convert
     */
    if (fbin == AR5416_BCHAN_UNUSED) {
        return fbin;
    }

    return (u_int16_t)((is2GHz) ? (2300 + fbin) : (4800 + 5 * fbin));
}

/* end shared */

void eepromDump_14_2(FILE *fd, ar5416_eeprom_t *eeprom);
const char *getTestGroupString(int type);


u_int16_t
eeread(u_int16_t off)
{
	u_int16_t eedata;

	atd.ad_id = HAL_DIAG_EEREAD | ATH_DIAG_IN | ATH_DIAG_DYN;
	atd.ad_in_size = sizeof(off);
	atd.ad_in_data = (caddr_t) &off;
	atd.ad_out_size = sizeof(eedata);
	atd.ad_out_data = (caddr_t) &eedata;
	if (ioctl(s, SIOCGATHDIAG, &atd) < 0)
		err(1, atd.ad_name);
	return eedata;
}

static void
eewrite(u_long off, u_long value)
{
	HAL_DIAG_EEVAL eeval;

	eeval.ee_off = (u_int16_t) off;
	eeval.ee_data = (u_int16_t) value;

	atd.ad_id = HAL_DIAG_EEWRITE | ATH_DIAG_IN;
	atd.ad_in_size = sizeof(eeval);
	atd.ad_in_data = (caddr_t) &eeval;
	atd.ad_out_size = 0;
	atd.ad_out_data = NULL;
	if (ioctl(s, SIOCGATHDIAG, &atd) < 0)
		err(1, atd.ad_name);
}

static void
usage()
{
	fprintf(stderr, "usage: %s [-i] [offset | offset=value]\n", progname);
	exit(-1);
}

int
main(int argc, char *argv[])
{
	HAL_REVS revs;
	const char *ifname = ATH_DEFAULT;
	int c;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "socket");

	progname = argv[0];
	while ((c = getopt(argc, argv, "i:")) != -1)
		switch (c) {
		case 'i':
			ifname = optarg;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	argc -= optind;
	argv += optind;

	strncpy(atd.ad_name, ifname, sizeof (atd.ad_name));

	atd.ad_id = HAL_DIAG_REVS;
	atd.ad_out_data = (caddr_t) &revs;
	atd.ad_out_size = sizeof(revs);
	if (ioctl(s, SIOCGATHDIAG, &atd) < 0)
		err(1, atd.ad_name);

	if (argc == 0) {
        /* TODO: need to share struct with hal */
        ar5416_eeprom_t eeprom;

		atd.ad_id = HAL_DIAG_EEPROM;
		atd.ad_out_data = (caddr_t) &eeprom;
		atd.ad_out_size = sizeof(eeprom);
		if (ioctl(s, SIOCGATHDIAG, &atd) < 0)
			err(1, atd.ad_name);

        eepromDump_14_2(stdout, &eeprom);

	} else {
		for (; argc > 0; argc--, argv++) {
			u_int16_t off, val, oval;
			char line[256];
			char *cp;

			cp = strchr(argv[0], '=');
			if (cp != NULL)
				*cp = '\0';
			off = (u_int16_t) strtoul(argv[0], 0, NULL);
			if (off == 0 && errno == EINVAL)
				errx(1, "%s: invalid eeprom offset %s",
					progname, argv[0]);
			if (cp == NULL) {
				printf("%04x: %04x\n", off, eeread(off));
			} else {
				val = (u_int16_t) strtoul(cp+1, 0, NULL);
				if (val == 0 && errno == EINVAL)
				errx(1, "%s: invalid eeprom value %s",
					progname, cp+1);
				oval = eeread(off);
				printf("Write %04x: %04x = %04x? ",
					off, oval, val);
				fflush(stdout);
				if (fgets(line, sizeof(line), stdin) != NULL &&
				    line[0] == 'y')
					eewrite(off, val);
			}
		}
	}
	return 0;
}



/**************************************************************
 *
 * Produce a formatted dump of the EEPROM structure
 */
void eepromDump_14_2(FILE *fd, ar5416_eeprom_t *ar5416Eep)
{
    u_int16_t          j, k, c, r;
    int16_t i;
    BASE_EEP_HEADER   *pBase = &(ar5416Eep->baseEepHeader);
    MODAL_EEP_HEADER  *pModal;
    CAL_TARGET_POWER_HT  *pPowerInfo = (CAL_TARGET_POWER_HT  *)0;
    CAL_TARGET_POWER_LEG  *pPowerInfoLeg = (CAL_TARGET_POWER_LEG  *)0;
    HAL_BOOL  legacyPowers = AH_FALSE;
    CAL_DATA_PER_FREQ *pDataPerChannel;
    HAL_BOOL            is2Ghz = 0;
    u_int8_t           noMoreSpurs;
    u_int8_t           *pChannel;
    u_int16_t          channelCount, channelRowCnt, vpdCount, rowEndsAtChan;
    u_int16_t          xpdGainMapping[] = {0, 1, 2, 4};
    u_int16_t          xpdGainValues[AR5416_NUM_PD_GAINS], numXpdGain = 0, xpdMask;
    u_int16_t          numPowers = 0, numRates = 0, ratePrint = 0, numChannels, tempFreq;
    u_int16_t          numTxChains = (u_int16_t)( ((pBase->txMask >> 2) & 1) +
        ((pBase->txMask >> 1) & 1) + (pBase->txMask & 1) );

    const static char *sRatePrint[3][8] = {
      {"     6-24     ", "      36      ", "      48      ", "      54      ", "", "", "", ""},
      {"       1      ", "       2      ", "     5.5      ", "      11      ", "", "", "", ""},
      {"   HT MCS0    ", "   HT MCS1    ", "   HT MCS2    ", "   HT MCS3    ",
       "   HT MCS4    ", "   HT MCS5    ", "   HT MCS6    ", "   HT MCS7    "},
    };

    const static char *sTargetPowerMode[7] = {
      "5G OFDM ", "5G HT20 ", "5G HT40 ", "2G CCK  ", "2G OFDM ", "2G HT20 ", "2G HT40 ",
    };

    const static char *sDeviceType[] = {
      "UNKNOWN [0] ",
      "Cardbus     ",
      "PCI         ",
      "MiniPCI     ",
      "Access Point",
      "PCIExpress  ",
      "UNKNOWN [6] ",
      "UNKNOWN [7] ",
    };

    const static char *sCtlType[] = {
        "[ 11A base mode ]",
        "[ 11B base mode ]",
        "[ 11G base mode ]",
        "[ INVALID       ]",
        "[ INVALID       ]",
        "[ 2G HT20 mode  ]",
        "[ 5G HT20 mode  ]",
        "[ 2G HT40 mode  ]",
        "[ 5G HT40 mode  ]",
    };

    if (!ar5416Eep) {
        return;
    }
// fprintf(fd, 

    /* Print Header info */
    pBase = &(ar5416Eep->baseEepHeader);
    fprintf(fd, "\n");
    fprintf(fd, "|===================== Header Information ====================|\n");
    fprintf(fd, "|  Major Version           %2d  |  Minor Version           %2d  |\n",
             pBase->version >> 12, pBase->version & 0xFFF);
    fprintf(fd, "|-------------------------------------------------------------|\n");
    fprintf(fd, "|  Checksum           0x%04X   |  Length             0x%04X   |\n",
             pBase->checksum, pBase->length);
    fprintf(fd, "|  RegDomain 1        0x%04X   |  RegDomain 2        0x%04X   |\n",
             pBase->regDmn[0], pBase->regDmn[1]);
    fprintf(fd, "|  TX Mask            0x%04X   |  RX Mask            0x%04X   |\n",
             pBase->txMask, pBase->rxMask);
    fprintf(fd, "|  rfSilent           0x%04X   |  btOptions          0x%04X   |\n",
             pBase->rfSilent, pBase->blueToothOptions);
    fprintf(fd, "|  deviceCap          0x%04X   |                              |\n",
             pBase->deviceCap);
    fprintf(fd, "|  MacAddress: 0x%02X:%02X:%02X:%02X:%02X:%02X                            |\n",
             pBase->macAddr[0], pBase->macAddr[1], pBase->macAddr[2],
             pBase->macAddr[3], pBase->macAddr[4], pBase->macAddr[5]);
    fprintf(fd, "|  OpFlags: [0x%2.2x] 11A %d, 11G %d                               |\n",
              pBase->opCapFlags,
             (pBase->opCapFlags & AR5416_OPFLAGS_11A) || 0,
             (pBase->opCapFlags & AR5416_OPFLAGS_11G) || 0);

    if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >= AR5416_EEP_MINOR_VER_3) {
        fprintf(fd, "|  OpFlags: Disable 5HT40 %d, Disable 2HT40 %d                     |\n",
            (pBase->opCapFlags & AR5416_OPFLAGS_N_5G_HT40) || 0,
            (pBase->opCapFlags & AR5416_OPFLAGS_N_2G_HT40) || 0);
        fprintf(fd, "|  OpFlags: Disable 5HT20 %d, Disable 2HT20 %d                     |\n",
            (pBase->opCapFlags & AR5416_OPFLAGS_N_5G_HT20) || 0,
            (pBase->opCapFlags & AR5416_OPFLAGS_N_2G_HT20) || 0);
    }

    fprintf(fd, "|  eepMisc: [0x%2.2x] endian %d                                   |\n",
              pBase->eepMisc,
              (pBase->eepMisc & AR5416_EEPMISC_BIG_ENDIAN) || 0);
    fprintf(fd, "|  Cal Bin Maj Ver %3d Cal Bin Min Ver %3d Cal Bin Build %3d  |\n",
             (pBase->binBuildNumber >> 24) & 0xFF,
             (pBase->binBuildNumber >> 16) & 0xFF,
             (pBase->binBuildNumber >> 8) & 0xFF);

    if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >= AR5416_EEP_MINOR_VER_3) {
    fprintf(fd, "|  Device Type: %s                                      |\n",
                 sDeviceType[(pBase->deviceType & 0x7)]);
    }

    fprintf(fd, "|  Customer Data in hex                                       |\n");
    for (i = 0; i < 64; i++) {
        if ((i % 16) == 0) {
            fprintf(fd, "|= ");
        }
        fprintf(fd, "%2.2X ", ar5416Eep->custData[i]);
        if ((i % 16) == 15) {
            fprintf(fd, "          =|\n");
        }
    }
    fprintf(fd, "|=============================================================|\n");

    /* Print Modal Header info */
    for (i = 0; i < 2; i++) {
        if ((i == 0) && !(pBase->opCapFlags & AR5416_OPFLAGS_11A)) {
            continue;
        }
        if ((i == 1) && !(pBase->opCapFlags & AR5416_OPFLAGS_11G)) {
            continue;
        }
        pModal = &(ar5416Eep->modalHeader[i]);
        if (i == 0) {
            fprintf(fd, "|=========== 5GHz Modal Header ===========|\n");
        } else {
            fprintf(fd, "|=========== 2GHz Modal Header ===========|\n");
        }
        fprintf(fd, "|  Ant Chain 0                0x%08X  |\n", pModal->antCtrlChain[0]);
        fprintf(fd, "|  Ant Chain 1                0x%08X  |\n", pModal->antCtrlChain[1]);
        fprintf(fd, "|  Ant Chain 2                0x%08X  |\n", pModal->antCtrlChain[2]);
        fprintf(fd, "|  Ant Chain common           0x%08X  |\n", pModal->antCtrlCommon);
        fprintf(fd, "|  Antenna Gain Chain 0              %3d  |\n", pModal->antennaGainCh[0]);
        fprintf(fd, "|  Antenna Gain Chain 1              %3d  |\n", pModal->antennaGainCh[1]);
        fprintf(fd, "|  Antenna Gain Chain 2              %3d  |\n", pModal->antennaGainCh[2]);
        fprintf(fd, "|  Switch Settling                   %3d  |\n", pModal->switchSettling);
        fprintf(fd, "|  TxRxAttenuation Ch 0              %3d  |\n", pModal->txRxAttenCh[0]);
        fprintf(fd, "|  TxRxAttenuation Ch 1              %3d  |\n", pModal->txRxAttenCh[1]);
        fprintf(fd, "|  TxRxAttenuation Ch 2              %3d  |\n", pModal->txRxAttenCh[2]);
        fprintf(fd, "|  RxTxMargin Chain 0                %3d  |\n", pModal->rxTxMarginCh[0]);
        fprintf(fd, "|  RxTxMargin Chain 1                %3d  |\n", pModal->rxTxMarginCh[1]);
        fprintf(fd, "|  RxTxMargin Chain 2                %3d  |\n", pModal->rxTxMarginCh[2]);
        fprintf(fd, "|  adc desired size                  %3d  |\n", pModal->adcDesiredSize);
        fprintf(fd, "|  pga desired size                  %3d  |\n", pModal->pgaDesiredSize);
        fprintf(fd, "|  xlna gain Chain 0                 %3d  |\n", pModal->xlnaGainCh[0]);
        fprintf(fd, "|  xlna gain Chain 1                 %3d  |\n", pModal->xlnaGainCh[1]);
        fprintf(fd, "|  xlna gain Chain 2                 %3d  |\n", pModal->xlnaGainCh[2]);
        fprintf(fd, "|  tx end to xpa off                 %3d  |\n", pModal->txEndToXpaOff);
        fprintf(fd, "|  tx end to rx on                   %3d  |\n", pModal->txEndToRxOn);
        fprintf(fd, "|  tx frame to xpa on                %3d  |\n", pModal->txFrameToXpaOn);
        fprintf(fd, "|  thresh62                          %3d  |\n", pModal->thresh62);
        fprintf(fd, "|  noise floor thres 0               %3d  |\n", pModal->noiseFloorThreshCh[0]);
        fprintf(fd, "|  noise floor thres 1               %3d  |\n", pModal->noiseFloorThreshCh[1]);
        fprintf(fd, "|  noise floor thres 2               %3d  |\n", pModal->noiseFloorThreshCh[2]);
        fprintf(fd, "|  Xpd Gain Mask                    0x%2.2X  |\n", pModal->xpdGain);
        fprintf(fd, "|  Xpd extern                         %2d  |\n", pModal->xpd);
        fprintf(fd, "|  IQ Cal I, Q Chain 0          %3d, %3d  |\n",
                    pModal->iqCalICh[0], pModal->iqCalQCh[0]);
        fprintf(fd, "|  IQ Cal I, Q Chain 1          %3d, %3d  |\n",
                    pModal->iqCalICh[1], pModal->iqCalQCh[1]);
        fprintf(fd, "|  IQ Cal I, Q Chain 2          %3d, %3d  |\n",
                    pModal->iqCalICh[2], pModal->iqCalQCh[2]);
        fprintf(fd, "|  pdGain Overlap                %2d.%d dB  |\n",
                    pModal->pdGainOverlap / 2, (pModal->pdGainOverlap % 2) * 5);
        fprintf(fd, "|  Analog Output Bias (ob)           %3d  |\n", pModal->ob);
        fprintf(fd, "|  Analog Driver Bias (db)           %3d  |\n", pModal->db);
        fprintf(fd, "|  Xpa bias level                    %3d  |\n", pModal->xpaBiasLvl);
        if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >= AR5416_EEP_MINOR_VER_7) {
            fprintf(fd, "|  Xpa bias level freq 0          %#6X  |\n", pModal->xpaBiasLvlFreq[0]);
            fprintf(fd, "|  Xpa bias level freq 1          %#6X  |\n", pModal->xpaBiasLvlFreq[1]);
            fprintf(fd, "|  Xpa bias level freq 2          %#6X  |\n", pModal->xpaBiasLvlFreq[2]);
        }
        fprintf(fd, "|  pwr dec 2 chain               %2d.%d dB  |\n", 
            pModal->pwrDecreaseFor2Chain / 2, (pModal->pwrDecreaseFor2Chain % 2) * 5);
        fprintf(fd, "|  pwr dec 3 chain               %2d.%d dB  |\n",
            pModal->pwrDecreaseFor3Chain / 2, (pModal->pwrDecreaseFor3Chain % 2) * 5);
        if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >= AR5416_EEP_MINOR_VER_2) {
            fprintf(fd, "|  txFrameToDataStart                %3d  |\n", pModal->txFrameToDataStart);
            fprintf(fd, "|  txFrameToPaOn                     %3d  |\n", pModal->txFrameToPaOn);
            fprintf(fd, "|  ht40PowerIncForPdadc              %3d  |\n", pModal->ht40PowerIncForPdadc);
        }

        if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >= AR5416_EEP_MINOR_VER_3) {
            fprintf(fd, "|  bswAtten Chain 0                  %3d  |\n", pModal->bswAtten[0]);
            fprintf(fd, "|  bswAtten Chain 1                  %3d  |\n", pModal->bswAtten[1]);
            fprintf(fd, "|  bswAtten Chain 2                  %3d  |\n", pModal->bswAtten[2]);
            fprintf(fd, "|  bswMargin Chain 0                 %3d  |\n", pModal->bswMargin[0]);
            fprintf(fd, "|  bswMargin Chain 1                 %3d  |\n", pModal->bswMargin[1]);
            fprintf(fd, "|  bswMargin Chain 2                 %3d  |\n", pModal->bswMargin[2]);
            fprintf(fd, "|  switch settling HT40              %3d  |\n", pModal->swSettleHt40);
        }
    }
    fprintf(fd, "|=========================================|\n");

#define SPUR_TO_KHZ(is2GHz, spur)    ( ((spur) + ((is2GHz) ? 23000 : 49000)) * 100 )
#define NO_SPUR                      ( 0x8000 )
    /* Print spur data */
    fprintf(fd, "|========================== Spur Information =========================|\n");
    for (i = 0; i < 2; i++) {
        if ((i == 0) && !(pBase->opCapFlags & AR5416_OPFLAGS_11A)) {
            continue;
        }
        if ((i == 1) && !(pBase->opCapFlags & AR5416_OPFLAGS_11G)) {
            continue;
        }
        if (i == 0) {
            fprintf(fd, "|      11A Spurs in MHz (Range of 0 defaults to channel width)        |\n");
        } else {
            fprintf(fd, "|      11G Spurs in MHz (Range of 0 defaults to channel width)        |\n");
        }
        pModal = &(ar5416Eep->modalHeader[i]);
        noMoreSpurs = 0;
        for (j = 0; j < AR5416_EEPROM_MODAL_SPURS; j++) {
            if ((pModal->spurChans[j].spurChan == NO_SPUR) || noMoreSpurs) {
                noMoreSpurs = 1;
                fprintf(fd, "|   NO SPUR   ");
            } else {
                fprintf(fd, "|    %4d.%1d   ", SPUR_TO_KHZ(i, pModal->spurChans[j].spurChan)/1000,
                         (SPUR_TO_KHZ(i, pModal->spurChans[j].spurChan)/100) % 10);
            }
        }
        fprintf(fd, "|\n");
        for (j = 0; j < AR5416_EEPROM_MODAL_SPURS; j++) {
            if ((pModal->spurChans[j].spurChan == NO_SPUR) || noMoreSpurs) {
                noMoreSpurs = 1;
                fprintf(fd, "|             ");
            } else {
                fprintf(fd, "|<%2d.%1d-=-%2d.%1d>", 
                       pModal->spurChans[j].spurRangeLow / 10, pModal->spurChans[j].spurRangeLow % 10,
                       pModal->spurChans[j].spurRangeHigh / 10, pModal->spurChans[j].spurRangeHigh % 10);
            }
        }
        fprintf(fd, "|\n");
    }
    fprintf(fd, "|=====================================================================|\n");
#undef SPUR_TO_KHZ
#undef NO_SPUR

    /* Print calibration info */
    for (i = 0; i < 2; i++) {
        if ((i == 0) && !(pBase->opCapFlags & AR5416_OPFLAGS_11A)) {
            continue;
        }
        if ((i == 1) && !(pBase->opCapFlags & AR5416_OPFLAGS_11G)) {
            continue;
        }
        for (c = 0; c < AR5416_MAX_CHAINS; c++) {
            if (!((pBase->txMask >> c) & 1)) {
                continue;
            }
            if (i == 0) {
                pDataPerChannel = ar5416Eep->calPierData5G[c];
                numChannels = AR5416_NUM_5G_CAL_PIERS;
                pChannel = ar5416Eep->calFreqPier5G;
            } else {
                pDataPerChannel = ar5416Eep->calPierData2G[c];
                numChannels = AR5416_NUM_2G_CAL_PIERS;
                pChannel = ar5416Eep->calFreqPier2G;
            }
            xpdMask = ar5416Eep->modalHeader[i].xpdGain;

            numXpdGain = 0;
            /* Calculate the value of xpdgains from the xpdGain Mask */
            for (j = 1; j <= AR5416_PD_GAINS_IN_MASK; j++) {
                if ((xpdMask >> (AR5416_PD_GAINS_IN_MASK - j)) & 1) {
                    if (numXpdGain >= AR5416_NUM_PD_GAINS) {
                        HALASSERT(0);
                        break;
                    }
                    xpdGainValues[numXpdGain++] = (u_int16_t)(AR5416_PD_GAINS_IN_MASK - j);
                }
            }

            fprintf(fd, "|================== Power Calibration Information Chain %1d =================|\n", c);
            fprintf(fd, "|==================             pdadc pwr(dBm)            =================|\n");
            for (channelRowCnt = 0; channelRowCnt < numChannels; channelRowCnt += 5) {
                rowEndsAtChan = (u_int16_t)AH_MIN(numChannels, (channelRowCnt + 5));
                for (channelCount = channelRowCnt; channelCount < rowEndsAtChan; channelCount++) {
                    fprintf(fd, "|     %04d     ", fbin2freq(pChannel[channelCount], (HAL_BOOL)i));
                }
                for (; channelCount < 5; channelCount++) {
                    fprintf(fd, "|              ");
                }
                fprintf(fd, "|\n|==============|==============|==============|==============|==============|\n");

                for (k = 0; k < numXpdGain; k++) {
                    fprintf(fd, "|              |              |              |              |              |\n");
                    fprintf(fd, "| PD_Gain %2d   |              |              |              |              |\n",
                             xpdGainMapping[xpdGainValues[k]]);

                    for (vpdCount = 0; vpdCount < AR5416_PD_GAIN_ICEPTS; vpdCount++) {
                        for (channelCount = channelRowCnt; channelCount < rowEndsAtChan; channelCount++) {
                            fprintf(fd, "| %3d   %2d.%02d  ", pDataPerChannel[channelCount].vpdPdg[k][vpdCount],
                                     pDataPerChannel[channelCount].pwrPdg[k][vpdCount] / 4,
                                     (pDataPerChannel[channelCount].pwrPdg[k][vpdCount] % 4) * 25);
                        }
                        for (; channelCount < 5; channelCount++) {
                            fprintf(fd, "|              ");
                        }
                        fprintf(fd, "|\n");
                    }
                }
                fprintf(fd, "|              |              |              |              |              |\n");
                fprintf(fd, "|==============|==============|==============|==============|==============|\n");
            }
        }
    }

    /* Print Target Powers */
    for (i = 0; i < 7; i++) {
        if ((i >=0 && i <=2) && !(pBase->opCapFlags & AR5416_OPFLAGS_11A)) {
            continue;
        }
        if ((i >= 3 && i <=6) && !(pBase->opCapFlags & AR5416_OPFLAGS_11G)) {
            continue;
        }
        switch(i) {
        case 0:
            pPowerInfo = (CAL_TARGET_POWER_HT *)ar5416Eep->calTargetPower5G;
            pPowerInfoLeg = ar5416Eep->calTargetPower5G;
            numPowers = AR5416_NUM_5G_20_TARGET_POWERS;
            ratePrint = 0;
            numRates = 4;
            is2Ghz = AH_FALSE;
            legacyPowers = AH_TRUE;
            break;
        case 1:
            pPowerInfo = ar5416Eep->calTargetPower5GHT20;
            numPowers = AR5416_NUM_5G_20_TARGET_POWERS;
            ratePrint = 2;
            numRates = 8;
            is2Ghz = AH_FALSE;
            legacyPowers = AH_FALSE;
            break;
        case 2:
            pPowerInfo = ar5416Eep->calTargetPower5GHT40;
            numPowers = AR5416_NUM_5G_40_TARGET_POWERS;
            ratePrint = 2;
            numRates = 8;
            is2Ghz = AH_FALSE;
            legacyPowers = AH_FALSE;
            break;
        case 3:
            pPowerInfo = (CAL_TARGET_POWER_HT *)ar5416Eep->calTargetPowerCck;
            pPowerInfoLeg = ar5416Eep->calTargetPowerCck;
            numPowers = AR5416_NUM_2G_CCK_TARGET_POWERS;
            ratePrint = 1;
            numRates = 4;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_TRUE;
            break;
        case 4:
            pPowerInfo = (CAL_TARGET_POWER_HT *)ar5416Eep->calTargetPower2G;
            pPowerInfoLeg = ar5416Eep->calTargetPower2G;
            numPowers = AR5416_NUM_2G_20_TARGET_POWERS;
            ratePrint = 0;
            numRates = 4;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_TRUE;
           break;
        case 5:
            pPowerInfo = ar5416Eep->calTargetPower2GHT20;
            numPowers = AR5416_NUM_2G_20_TARGET_POWERS;
            ratePrint = 2;
            numRates = 8;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_FALSE;
            break;
        case 6:
            pPowerInfo = ar5416Eep->calTargetPower2GHT40;
            numPowers = AR5416_NUM_2G_40_TARGET_POWERS;
            ratePrint = 2;
            numRates = 8;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_FALSE;
            break;
        }
        fprintf(fd, "============================Target Power Info===============================\n");
        for (j = 0; j < numPowers; j+=4) {
            fprintf(fd, "|   %s   ", sTargetPowerMode[i]);
            for (k = j; k < AH_MIN(j + 4, numPowers); k++) {
                if(legacyPowers) {
                    fprintf(fd, "|     %04d     ",fbin2freq(pPowerInfoLeg[k].bChannel, (HAL_BOOL)i));
                } else {
                    fprintf(fd, "|     %04d     ",fbin2freq(pPowerInfo[k].bChannel, (HAL_BOOL)i));
                }
            }
            fprintf(fd, "|\n");
            fprintf(fd, "|==============|==============|==============|==============|==============|\n");
            for (r = 0; r < numRates; r++) {
                fprintf(fd, "|%s", sRatePrint[ratePrint][r]);
                for (k = j; k < AH_MIN(j + 4, numPowers); k++) {
                    if(legacyPowers) {
                        fprintf(fd, "|     %2d.%d     ", pPowerInfoLeg[k].tPow2x[r] / 2,
                             (pPowerInfo[k].tPow2x[r] % 2) * 5);
                    } else {
                        fprintf(fd, "|     %2d.%d     ", pPowerInfo[k].tPow2x[r] / 2,
                             (pPowerInfo[k].tPow2x[r] % 2) * 5);
                    }
                }
                fprintf(fd, "|\n");
            }
            fprintf(fd, "|==============|==============|==============|==============|==============|\n");
        }
    }
    fprintf(fd, "\n");

    /* Print Band Edge Powers */
    fprintf(fd, "=======================Test Group Band Edge Power========================\n");
    for (i = 0; (ar5416Eep->ctlIndex[i] != 0) && (i < AR5416_NUM_CTLS); i++) {
        fprintf(fd, "|                                                                       |\n");
        fprintf(fd, "| CTL: 0x%02x %s %s                                  |\n",
                 ar5416Eep->ctlIndex[i],
                 getTestGroupString(ar5416Eep->ctlIndex[i] & 0xf0),
                 sCtlType[ar5416Eep->ctlIndex[i] & CTL_MODE_M]);
        fprintf(fd, "|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

        for (c = 0; c < numTxChains; c++) {
            fprintf(fd, "======================== Edges for Chain %d ==============================\n", c);
            fprintf(fd, "| edge  ");
            for (j = 0; j < AR5416_NUM_BAND_EDGES; j++) {
                if (ar5416Eep->ctlData[i].ctlEdges[c][j].bChannel == AR5416_BCHAN_UNUSED) {
                    fprintf(fd, "|  --   ");
                } else {
                    tempFreq = fbin2freq(ar5416Eep->ctlData[i].ctlEdges[c][j].bChannel, is2Ghz);
                    fprintf(fd, "| %04d  ", tempFreq);
                }
            }
            fprintf(fd, "|\n");
            fprintf(fd, "|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

            fprintf(fd, "| power ");
            for (j = 0; j < AR5416_NUM_BAND_EDGES; j++) {
                if (ar5416Eep->ctlData[i].ctlEdges[c][j].bChannel == AR5416_BCHAN_UNUSED) {
                    fprintf(fd, "|  --   ");
                } else {
                    fprintf(fd, "| %2d.%d  ", ar5416Eep->ctlData[i].ctlEdges[c][j].tPower / 2,
                        (ar5416Eep->ctlData[i].ctlEdges[c][j].tPower % 2) * 5);
                }
            }
            fprintf(fd, "|\n");
            fprintf(fd, "|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

            fprintf(fd, "| flag  ");
            for (j = 0; j < AR5416_NUM_BAND_EDGES; j++) {
                if (ar5416Eep->ctlData[i].ctlEdges[c][j].bChannel == AR5416_BCHAN_UNUSED) {
                    fprintf(fd, "|  --   ");
                } else {
                    fprintf(fd, "|   %1d   ", ar5416Eep->ctlData[i].ctlEdges[c][j].flag);
                }
            }
            fprintf(fd, "|\n");
            fprintf(fd, "=========================================================================\n");
        }
    }

}

const static char *sTestGroupType_FCC  = "[ FCC ] "; // 0x10
const static char *sTestGroupType_MKK  = "[ MKK ] "; // 0x40
const static char *sTestGroupType_ETSI = "[ ETSI ]"; // 0x30

#define FCC_TYPE        0x10
#define MKK_TYPE        0x40
#define ETSI_TYPE       0x30

const char *getTestGroupString(int type)
{
    switch(type) {
    case FCC_TYPE:
        return sTestGroupType_FCC;
        break;
    case MKK_TYPE:
        return sTestGroupType_MKK;
        break;
    case ETSI_TYPE:
        return sTestGroupType_ETSI;
        break;
    default:
        return "";
    }
}
