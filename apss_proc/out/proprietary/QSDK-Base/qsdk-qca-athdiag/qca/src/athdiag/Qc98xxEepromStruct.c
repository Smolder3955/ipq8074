/*
 * Copyright (c) 2017 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include "Qc98xxEepromStruct.h"

const char *sRatePrintHT[30] =
{
    "MCS0 ",
    "MCS1 ",
    "MCS2 ",
    "MCS3 ",
    "MCS4 ",
    "MCS5 ",
    "MCS6 ",
    "MCS7 ",
    "MCS8 ",
    "MCS9 ",
    "MCS10",
    "MCS11",
    "MCS12",
    "MCS13",
    "MCS14",
    "MCS15",
    "MCS16",
    "MCS17",
    "MCS18",
    "MCS19",
    "MCS20",
    "MCS21",
    "MCS22",
    "MCS23",
    "MCS24",
    "MCS25",
    "MCS26",
    "MCS27",
    "MCS28",
    "MCS29",
    };

const char *sRatePrintVHT[30] =
{
    "MCS0_1SS",
    "MCS1_1SS",
    "MCS2_1SS",
    "MCS3_1SS",
    "MCS4_1SS",
    "MCS5_1SS",
    "MCS6_1SS",
    "MCS7_1SS",
    "MCS8_1SS",
    "MCS9_1SS",
    "MCS0_2SS",
    "MCS1_2SS",
    "MCS2_2SS",
    "MCS3_2SS",
    "MCS4_2SS",
    "MCS5_2SS",
    "MCS6_2SS",
    "MCS7_2SS",
    "MCS8_2SS",
    "MCS9_2SS",
    "MCS0_3SS",
    "MCS1_3SS",
    "MCS2_3SS",
    "MCS3_3SS",
    "MCS4_3SS",
    "MCS5_3SS",
    "MCS6_3SS",
    "MCS7_3SS",
    "MCS8_3SS",
    "MCS9_3SS",
    };

const char *sRatePrintLegacy[4] =
{
    "6-24",
    " 36 ",
    " 48 ",
    " 54 "
};

const char *sRatePrintCck[4] =
{
    "1L-5L",
    " 5S  ",
    " 11L ",
    " 11S "
};


const char *sDeviceType[] =
{
    "UNKNOWN",
    "Cardbus",
    "PCI    ",
    "MiniPCI",
    "AP     ",
    "PCIE   ",
    "UNKNOWN",
    "UNKNOWN",
    "USB    ",
};

const char *sCtlType[] =
{
    "[ 11A base mode ]",
    "[ 11B base mode ]",
    "[ 11G base mode ]",
    "[ INVALID       ]",
    "[ INVALID       ]",
    "[ 2G HT20 mode  ]",
    "[ 5G HT20 mode  ]",
    "[ 2G HT40 mode  ]",
    "[ 5G HT40 mode  ]",
    "[ 5G VHT80 mode ]",
};

const int mapRate2Index[24]=
{
    0,1,1,1,2,
    3,4,5,0,1,
    1,1,6,7,8,
    9,0,1,1,1,
    10,11,12,13
};




