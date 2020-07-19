/*
 *  Copyright (c) 2000-2002 Atheros Communications, Inc., All Rights Reserved 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */



#include <osdep.h>
#include "ratectrl.h"
#include "ratectrl11n.h"

#define SHORT_PRE 1
#define LONG_PRE 0

#define WLAN_PHY_HT_20_SS       WLAN_RC_PHY_HT_20_SS
#define WLAN_PHY_HT_20_SS_HGI   WLAN_RC_PHY_HT_20_SS_HGI
#define WLAN_PHY_HT_20_DS       WLAN_RC_PHY_HT_20_DS
#define WLAN_PHY_HT_20_DS_HGI   WLAN_RC_PHY_HT_20_DS_HGI
#define WLAN_PHY_HT_20_TS       WLAN_RC_PHY_HT_20_TS
#define WLAN_PHY_HT_20_TS_HGI   WLAN_RC_PHY_HT_20_TS_HGI
#define WLAN_PHY_HT_40_SS       WLAN_RC_PHY_HT_40_SS
#define WLAN_PHY_HT_40_SS_HGI   WLAN_RC_PHY_HT_40_SS_HGI
#define WLAN_PHY_HT_40_DS       WLAN_RC_PHY_HT_40_DS
#define WLAN_PHY_HT_40_DS_HGI   WLAN_RC_PHY_HT_40_DS_HGI
#define WLAN_PHY_HT_40_TS       WLAN_RC_PHY_HT_40_TS
#define WLAN_PHY_HT_40_TS_HGI   WLAN_RC_PHY_HT_40_TS_HGI



#ifndef ATH_NO_5G_SUPPORT
static RATE_INFO_11N ar5416_11naRateTableInfo[] = {

     /*                               Triple(T)                                                                                                                                                   */
     /*              Triple(T)        Dual(D)         Valid    Valid                                                                                                                              */
     /*              Dual(D)          Single(S)       for      for                                                          rate  short   dot11 ctrl RssiAck  RssiAck  maxtx base  cw40  sgi    ht  4ms tx */
     /*              Single(S) strm   strm STBC       UAPSD    TxBF                                          Kbps   uKbps   Code  Preamb  Rate  Rate ValidMin DeltaMin chain Idx   Idx   Idx   Idx  limit  */
     /*    6 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     TRUE,    FALSE,               WLAN_PHY_OFDM,           6000,   5200,  0x0b,  0x00,   12,   0,    2,       1,     3,     0,    0,    0,    0,      0},
     /*    9 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     FALSE,   FALSE,               WLAN_PHY_OFDM,           9000,   7500,  0x0f,  0x00,   18,   0,    3,       1,     3,     1,    1,    1,    1,      0},
     /*   12 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     TRUE,    FALSE,               WLAN_PHY_OFDM,          12000,   9700,  0x0a,  0x00,   24,   2,    4,       2,     3,     2,    2,    2,    2,      0},
     /*   18 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     FALSE,   FALSE,               WLAN_PHY_OFDM,          18000,  13600,  0x0e,  0x00,   36,   2,    6,       2,     3,     3,    3,    3,    3,      0},
     /*   24 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     TRUE,    FALSE,               WLAN_PHY_OFDM,          24000,  17000,  0x09,  0x00,   48,   4,   10,       3,     3,     4,    4,    4,    4,      0},
     /*   36 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     FALSE,   FALSE,               WLAN_PHY_OFDM,          36000,  22700,  0x0d,  0x00,   72,   4,   14,       3,     3,     5,    5,    5,    5,      0},
     /*   48 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     FALSE,   FALSE,               WLAN_PHY_OFDM,          48000,  27100,  0x08,  0x00,   96,   4,   20,       3,     1,     6,    6,    6,    6,      0},
     /*   54 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     TRUE,    FALSE,               WLAN_PHY_OFDM,          54000,  28900,  0x0c,  0x00,  108,   4,   23,       3,     1,     7,    7,    7,    7,      0},
     /*  6.5 Mb */ {  TRUE2040_1_2_3, TRUE2040_1_2_3, TRUE,    TRUE2040_N_1_2_ALL,  WLAN_PHY_HT_20_SS,       6500,   5800,  0x80,  0x00,    0,   0,    2,       3,     3,     8,   38,    8,   38,   3210},
     /*   13 Mb */ {  TRUE20_1_2_3,   TRUE20_1_2_3,   TRUE,    TRUE20_N_1_2_ALL,    WLAN_PHY_HT_20_SS,      13000,  11700,  0x81,  0x00,    1,   2,    4,       3,     3,     9,   39,    9,   39,   6430},
     /* 19.5 Mb */ {  TRUE20_1_2_3,   TRUE20_1_2_3,   FALSE,   TRUE20_N_1_2_ALL,    WLAN_PHY_HT_20_SS,      19500,  17600,  0x82,  0x00,    2,   2,    6,       3,     3,    10,   40,   10,   40,   9650},
     /*   26 Mb */ {  TRUE20_1_2,     TRUE20_1_2,     FALSE,   TRUE20_N_1_2_ALL,    WLAN_PHY_HT_20_SS,      26000,  23500,  0x83,  0x00,    3,   4,   10,       3,     3,    11,   41,   11,   41,  12880},
     /*   39 Mb */ {  TRUE20_1_2,     TRUE20_1_2,     TRUE,    TRUE20_N_1_2_ALL,    WLAN_PHY_HT_20_SS,      39000,  35300,  0x84,  0x00,    4,   4,   14,       3,     3,    12,   42,   12,   42,  19320},
     /*   52 Mb */ {  TRUE20_1,       TRUE20_1_2,     FALSE,   TRUE20_N2_F_N1_D_S,  WLAN_PHY_HT_20_SS,      52000,  47100,  0x85,  0x00,    5,   4,   20,       3,     2,    13,   43,   13,   43,  25760},
     /* 58.5 Mb */ {  TRUE20_1,       TRUE20_1_2,     FALSE,   TRUE20_N2_F_N1_D_S,  WLAN_PHY_HT_20_SS,      58500,  52900,  0x86,  0x00,    6,   4,   23,       3,     2,    14,   44,   14,   44,  28980},
     /*   65 Mb */ {  TRUE20_1,       TRUE20_1,       TRUE,    TRUE20_N2_F_N1_S,    WLAN_PHY_HT_20_SS,      65000,  58800,  0x87,  0x00,    7,   4,   25,       3,     2,    15,   45,   16,   46,  32200},
     /*   75 Mb */ {  TRUE20_1,       TRUE20_1,       TRUE,    TRUE20_N2_F_N1_S,    WLAN_PHY_HT_20_SS_HGI,  72200,  65400,  0x87,  0x00,    7,   4,   25,       3,     2,    15,   45,   16,   46,  35780},
     /*   13 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    TRUE,    FALSE,               WLAN_PHY_HT_20_DS,      13000,  11600,  0x88,  0x00,    8,   0,    2,       3,     3,    16,   47,   17,   47,   6430},
     /*   26 Mb */ {  TRUE20_3,       TRUE20_3,       FALSE,   FALSE,               WLAN_PHY_HT_20_DS,      26000,  23400,  0x89,  0x00,    9,   2,    4,       3,     3,    17,   48,   18,   48,  12860},
     /*   39 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    FALSE,               WLAN_PHY_HT_20_DS,      39000,  35200,  0x8a,  0x00,   10,   2,    6,       3,     3,    18,   49,   19,   49,  19300},
     /*   52 Mb */ {  TRUE20_2_3,     TRUE20_3,       FALSE,   TRUE20_N2_ALL_N1_T,  WLAN_PHY_HT_20_DS,      52000,  47000,  0x8b,  0x00,   11,   4,   10,       3,     3,    19,   50,   20,   50,  25730},
     /*   78 Mb */ {  TRUE20_2_3,     TRUE20_2_3,     TRUE,    TRUE20_N2_ALL_N1_T_D,WLAN_PHY_HT_20_DS,      78000,  70500,  0x8c,  0x00,   12,   4,   14,       3,     3,    20,   51,   21,   51,  38600},
     /*  104 Mb */ {  TRUE20_2_3,     TRUE20_2_3,     FALSE,   TRUE20_N2_ALL_N1_T_D,WLAN_PHY_HT_20_DS,     104000,  94000,  0x8d,  0x00,   13,   4,   20,       3,     2,    21,   52,   22,   52,  51470},
     /*  117 Mb */ {  TRUE20_2,       TRUE20_2,       FALSE,   TRUE20_N2_D_N1_D,    WLAN_PHY_HT_20_DS,     117000, 105200,  0x8e,  0x00,   14,   4,   23,       3,     2,    22,   53,   23,   53,  57910},
     /*  130 Mb */ {  TRUE20_2,       TRUE20_2,       TRUE,    TRUE20_N2_D_N1_D,    WLAN_PHY_HT_20_DS,     130000, 116100,  0x8f,  0x00,   15,   4,   25,       3,     2,    23,   54,   25,   55,  64340},
     /*144.4 Mb */ {  TRUE20_2,       TRUE20_2,       TRUE,    TRUE20_N2_D_N1_D,    WLAN_PHY_HT_20_DS_HGI, 144400, 128100,  0x8f,  0x00,   15,   4,   25,       3,     2,    23,   54,   25,   55,  71490},
     /* 19.5 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_20_TS,      19500,  17400,  0x90,  0x00,   16,   0,   25,       3,     3,    24,   56,   26,   56,   9630},
     /*   39 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_20_TS,      39000,  35100,  0x91,  0x00,   17,   2,   25,       3,     3,    25,   57,   27,   57,  19260},
     /* 58.5 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_20_TS,      58500,  52600,  0x92,  0x00,   18,   2,   25,       3,     3,    26,   58,   28,   58,  28890},
     /*   78 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_20_TS,      78000,  70400,  0x93,  0x00,   19,   4,   25,       3,     3,    27,   59,   29,   59,  38520},
     /*  117 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS,     117000, 104900,  0x94,  0x00,   20,   4,   25,       3,     3,    28,   60,   31,   61,  57790},
     /*  130 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS_HGI, 130000, 115800,  0x94,  0x00,   20,   4,   25,       3,     3,    28,   60,   31,   61,  64210},
     /*  156 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS,     156000, 137200,  0x95,  0x00,   21,   4,   25,       3,     3,    29,   62,   33,   63,  77060},
     /*173.3 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS_HGI, 173300, 151100,  0x95,  0x00,   21,   4,   25,       3,     3,    29,   62,   33,   63,  85620},
     /*175.5 Mb */ {  TRUE20_3,       TRUE20_3,       FALSE,   TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS,     175500, 152800,  0x96,  0x00,   22,   4,   25,       3,     3,    30,   64,   35,   65,  86690},
     /*  195 Mb */ {  TRUE20_3,       TRUE20_3,       FALSE,   TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS_HGI, 195000, 168400,  0x96,  0x00,   22,   4,   25,       3,     3,    30,   64,   35,   65,  96320},
     /*  195 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS,     195000, 168400,  0x97,  0x00,   23,   4,   25,       3,     3,    31,   66,   37,   67,  96320},
     /*216.7 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS_HGI, 216700, 185000,  0x97,  0x00,   23,   4,   25,       3,     3,    31,   66,   37,   67, 107030},
     /* 13.5 Mb */ {  TRUE40_1_2_3,   TRUE40_1_2_3,   TRUE,    TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      13500,  12100,  0x80,  0x00,    0,   0,    2,       3,     3,     8,   38,   38,   38,   6680},
     /* 27.0 Mb */ {  TRUE40_1_2_3,   TRUE40_1_2_3,   TRUE,    TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      27000,  24300,  0x81,  0x00,    1,   2,    4,       3,     3,     9,   39,   39,   39,  13370},
     /* 40.5 Mb */ {  TRUE40_1_2_3,   TRUE40_1_2_3,   FALSE,   TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      40500,  36500,  0x82,  0x00,    2,   2,    6,       3,     3,    10,   40,   40,   40,  20060},
     /*   54 Mb */ {  TRUE40_1_2,     TRUE40_1_2,     FALSE,   TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      54000,  48900,  0x83,  0x00,    3,   4,   10,       3,     3,    11,   41,   41,   41,  26750},
     /*   81 Mb */ {  TRUE40_1_2,     TRUE40_1_2,     TRUE,    TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      81000,  73300,  0x84,  0x00,    4,   4,   14,       3,     3,    12,   42,   42,   42,  40130},
     /*  108 Mb */ {  TRUE40_1,       TRUE40_1_2,     FALSE,   TRUE40_N2_F_N1_D_S,  WLAN_PHY_HT_40_SS,     108000,  97500,  0x85,  0x00,    5,   4,   20,       3,     1,    13,   43,   43,   43,  53510},
     /* 121.5Mb */ {  TRUE40_1,       TRUE40_1_2,     FALSE,   TRUE40_N2_F_N1_D_S,  WLAN_PHY_HT_40_SS,     121500, 109100,  0x86,  0x00,    6,   4,   23,       3,     1,    14,   44,   44,   44,  60200},
     /*  135 Mb */ {  TRUE40_1,       TRUE40_1,       TRUE,    TRUE40_N2_F_N1_S,    WLAN_PHY_HT_40_SS,     135000, 120400,  0x87,  0x00,    7,   4,   25,       3,     1,    15,   45,   46,   46,  66880},
     /*  150 Mb */ {  TRUE40_1,       TRUE40_1,       TRUE,    TRUE40_N2_F_N1_S,    WLAN_PHY_HT_40_SS_HGI, 150000, 133000,  0x87,  0x00,    7,   4,   25,       3,     1,    15,   45,   46,   46,  74320},
     /*   27 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    TRUE,    FALSE,               WLAN_PHY_HT_40_DS,      27000,  24100,  0x88,  0x00,    8,   0,    2,       3,     3,    16,   47,   47,   47,  13360},
     /*   54 Mb */ {  TRUE40_3,       TRUE40_3,       FALSE,   FALSE,               WLAN_PHY_HT_40_DS,      54000,  48700,  0x89,  0x00,    9,   2,    4,       3,     3,    17,   48,   48,   48,  26720},
     /*   81 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    FALSE,               WLAN_PHY_HT_40_DS,      81000,  73000,  0x8a,  0x00,   10,   2,    6,       3,     3,    18,   49,   49,   49,  40090},
     /*  108 Mb */ {  TRUE40_2_3,     TRUE40_3,       FALSE,   TRUE40_N2_ALL_N1_T,  WLAN_PHY_HT_40_DS,     108000,  97400,  0x8b,  0x00,   11,   4,   10,       3,     3,    19,   50,   50,   50,  53450},
     /*  162 Mb */ {  TRUE40_2_3,     TRUE40_2_3,     TRUE,    TRUE40_N2_ALL_N1_T_D,WLAN_PHY_HT_40_DS,     162000, 142400,  0x8c,  0x00,   12,   4,   14,       3,     3,    20,   51,   51,   51,  80180},
     /*  216 Mb */ {  TRUE40_2_3,     TRUE40_2_3,     FALSE,   TRUE40_N2_ALL_N1_T_D,WLAN_PHY_HT_40_DS,     216000, 185300,  0x8d,  0x00,   13,   4,   20,       3,     2,    21,   52,   52,   52, 106910},
     /*  243 Mb */ {  TRUE40_2,       TRUE40_2,       FALSE,   TRUE40_N2_D_N1_D,    WLAN_PHY_HT_40_DS,     243000, 206000,  0x8e,  0x00,   14,   4,   23,       3,     2,    22,   53,   53,   53, 120280},
     /*  270 Mb */ {  TRUE40_2,       TRUE40_2,       TRUE,    TRUE40_N2_D_N1_D,    WLAN_PHY_HT_40_DS,     270000, 225800,  0x8f,  0x00,   15,   4,   25,       3,     2,    23,   54,   55,   55, 133640},
     /*  300 Mb */ {  TRUE40_2,       TRUE40_2,       TRUE,    TRUE40_N2_D_N1_D,    WLAN_PHY_HT_40_DS_HGI, 300000, 247800,  0x8f,  0x00,   15,   4,   25,       3,     2,    23,   54,   55,   55, 148490},
     /* 40.5 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_40_TS,      40500,  36100,  0x90,  0x00,   16,   0,   25,       3,     3,    24,   56,   56,   56,  20000},
     /*   81 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_40_TS,      81000,  72900,  0x91,  0x00,   17,   2,   25,       3,     3,    25,   57,   57,   57,  40010},
     /*121.5 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_40_TS,     121500, 108300,  0x92,  0x00,   18,   2,   25,       3,     3,    26,   58,   58,   58,  60010},
     /*  162 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_40_TS,     162000, 142000,  0x93,  0x00,   19,   4,   25,       3,     3,    27,   59,   59,   59,  80020},
     /*  243 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS,     243000, 205100,  0x94,  0x00,   20,   4,   25,       3,     3,    28,   60,   61,   61, 120030},
     /*  270 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS_HGI, 270000, 224700,  0x94,  0x00,   20,   4,   25,       3,     3,    28,   60,   61,   61, 133370},
     /*  324 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS,     324000, 263100,  0x95,  0x00,   21,   4,   25,       3,     3,    29,   62,   63,   63, 160050},
     /*  360 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS_HGI, 360000, 288000,  0x95,  0x00,   21,   4,   25,       3,     3,    29,   62,   63,   63, 177830},
     /*364.5 Mb */ {  TRUE40_3,       TRUE40_3,       FALSE,   TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS,     364500, 290700,  0x96,  0x00,   22,   4,   25,       3,     3,    30,   64,   65,   65, 180060},
     /*  405 Mb */ {  TRUE40_3,       TRUE40_3,       FALSE,   TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS_HGI, 405000, 317200,  0x96,  0x00,   22,   4,   25,       3,     3,    30,   64,   65,   65, 200060},
     /*  405 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS,     405000, 317200,  0x97,  0x00,   23,   4,   25,       3,     3,    31,   66,   67,   67, 200060},
     /*  450 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS_HGI, 450000, 346400,  0x97,  0x00,   23,   4,   25,       3,     3,    31,   66,   67,   67, 222290},
};

static RATE_TABLE_11N ar5416_11naRateTable = {
    sizeof(ar5416_11naRateTableInfo)/sizeof(RATE_INFO_11N),  /* number of rates */
    ar5416_11naRateTableInfo,
    50,  /* probe interval */
    50,  /* rssi reduce interval */
    WLAN_RC_HT_FLAG,  /* Phy rates allowed initially */
};
#endif /* #ifndef ATH_NO_5G_SUPPORT */

    /* TRUE_ALL - valid for 20/40/Legacy, TRUE - Legacy only, TRUE_20 - HT 20 only, TRUE_40 - HT 40 only */
    /* 4ms frame limit not used for NG mode.  The values filled for HT are the 64K max aggregate limit */




	 static RATE_INFO_11N ar5416_11ngRateTableInfo[] = {
     /*                               Triple(T)                                                                                                                                                   */
     /*              Triple(T)        Dual(D)         Valid    Valid                                                                                                                              */
     /*              Dual(D)          Single(S)       for      for                                                                short   dot11 ctrl RssiAck  RssiAck  maxtx base cw40   sgi   ht   4ms tx */
     /*              Single(S) strm   strm STBC       UAPSD    TxBF                                          Kbps    uKbps   RC   Preamb  Rate  Rate ValidMin DeltaMin chain Idx  Idx    Idx   Idx  limit  */
     /*    1 Mb */ {  TRUE_ALL_1_2_3, TRUE_ALL_1_2_3, TRUE,    FALSE,               WLAN_PHY_CCK,            1000,    900,  0x1b,  0x00,    2,   0,    0,       1,     3,     0,    0,    0,    0,      0},
     /*    2 Mb */ {  TRUE_ALL_1_2_3, TRUE_ALL_1_2_3, FALSE,   FALSE,               WLAN_PHY_CCK,            2000,   1900,  0x1a,  0x04,    4,   1,    1,       1,     3,     1,    1,    1,    1,      0},
     /*  5.5 Mb */ {  TRUE_ALL_1_2_3, TRUE_ALL_1_2_3, FALSE,   FALSE,               WLAN_PHY_CCK,            5500,   4900,  0x19,  0x04,   11,   2,    2,       2,     3,     2,    2,    2,    2,      0},
     /*   11 Mb */ {  TRUE_ALL_1_2_3, TRUE_ALL_1_2_3, TRUE,    FALSE,               WLAN_PHY_CCK,           11000,   8100,  0x18,  0x04,   22,   3,    3,       2,     3,     3,    3,    3,    3,      0},
     /*    6 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_OFDM,           6000,   5200,  0x0b,  0x00,   12,   4,    2,       1,     3,     4,    4,    4,    4,      0},
     /*    9 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_OFDM,           9000,   7500,  0x0f,  0x00,   18,   4,    3,       1,     3,     5,    5,    5,    5,      0},
     /*   12 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     FALSE,   FALSE,               WLAN_PHY_OFDM,          12000,   9700,  0x0a,  0x00,   24,   6,    4,       1,     3,     6,    6,    6,    6,      0},
     /*   18 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     FALSE,   FALSE,               WLAN_PHY_OFDM,          18000,  13600,  0x0e,  0x00,   36,   6,    6,       2,     3,     7,    7,    7,    7,      0},
     /*   24 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     TRUE,    FALSE,               WLAN_PHY_OFDM,          24000,  17000,  0x09,  0x00,   48,   8,   10,       3,     3,     8,    8,    8,    8,      0},
     /*   36 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     FALSE,   FALSE,               WLAN_PHY_OFDM,          36000,  22700,  0x0d,  0x00,   72,   8,   14,       3,     3,     9,    9,    9,    9,      0},
     /*   48 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     FALSE,   FALSE,               WLAN_PHY_OFDM,          48000,  27100,  0x08,  0x00,   96,   8,   20,       3,     1,    10,   10,   10,   10,      0},
     /*   54 Mb */ {  TRUE_1_2_3,     TRUE_1_2_3,     TRUE,    FALSE,               WLAN_PHY_OFDM,          54000,  28900,  0x0c,  0x00,  108,   8,   23,       3,     1,    11,   11,   11,   11,      0},
     /*  6.5 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    TRUE,    TRUE20_N_1_2_ALL,    WLAN_PHY_HT_20_SS,       6500,   5800,  0x80,  0x00,    0,   4,    2,       3,     3,    12,   42,   12,   42,   3210},
     /*   13 Mb */ {  TRUE20_1_2_3,   TRUE20_1_2_3,   TRUE,    TRUE20_N_1_2_ALL,    WLAN_PHY_HT_20_SS,      13000,  11700,  0x81,  0x00,    1,   6,    4,       3,     3,    13,   43,   13,   43,   6430},
     /* 19.5 Mb */ {  TRUE20_1_2_3,   TRUE20_1_2_3,   FALSE,   TRUE20_N_1_2_ALL,    WLAN_PHY_HT_20_SS,      19500,  17600,  0x82,  0x00,    2,   6,    6,       3,     3,    14,   44,   14,   44,   9650},
     /*   26 Mb */ {  TRUE20_1_2,     TRUE20_1_2,     FALSE,   TRUE20_N_1_2_ALL,    WLAN_PHY_HT_20_SS,      26000,  23500,  0x83,  0x00,    3,   8,   10,       3,     3,    15,   45,   15,   45,  12880},
     /*   39 Mb */ {  TRUE20_1_2,     TRUE20_1_2,     TRUE,    TRUE20_N_1_2_ALL,    WLAN_PHY_HT_20_SS,      39000,  35300,  0x84,  0x00,    4,   8,   14,       3,     3,    16,   46,   16,   46,  19320},
     /*   52 Mb */ {  TRUE20_1,       TRUE20_1_2,     FALSE,   TRUE20_N2_F_N1_D_S,  WLAN_PHY_HT_20_SS,      52000,  47100,  0x85,  0x00,    5,   8,   20,       3,     1,    17,   47,   17,   47,  25760},
     /* 58.5 Mb */ {  TRUE20_1,       TRUE20_1_2,     FALSE,   TRUE20_N2_F_N1_D_S,  WLAN_PHY_HT_20_SS,      58500,  52900,  0x86,  0x00,    6,   8,   23,       3,     1,    18,   48,   18,   48,  28980},
     /*   65 Mb */ {  TRUE20_1,       TRUE20_1,       TRUE,    TRUE20_N2_F_N1_S,    WLAN_PHY_HT_20_SS,      65000,  58800,  0x87,  0x00,    7,   8,   25,       3,     1,    19,   49,   20,   50,  32200},
     /*   75 Mb */ {  TRUE20_1,       TRUE20_1,       TRUE,    TRUE20_N2_F_N1_S,    WLAN_PHY_HT_20_SS_HGI,  72200,  65400,  0x87,  0x00,    7,   8,   25,       3,     2,    19,   49,   20,   50,  35780},
     /*   13 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    TRUE,    FALSE,               WLAN_PHY_HT_20_DS,      13000,  11600,  0x88,  0x00,    8,   4,    2,       3,     3,    20,   51,   21,   51,   6430},
     /*   26 Mb */ {  TRUE20_3,       TRUE20_3,       FALSE,   FALSE,               WLAN_PHY_HT_20_DS,      26000,  23400,  0x89,  0x00,    9,   6,    4,       3,     3,    21,   52,   22,   52,  12860},
     /*   39 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    FALSE,               WLAN_PHY_HT_20_DS,      39000,  35200,  0x8a,  0x00,   10,   6,    6,       3,     3,    22,   53,   23,   53,  19300},
     /*   52 Mb */ {  TRUE20_2_3,     TRUE20_3,       FALSE,   TRUE20_N2_ALL_N1_T,  WLAN_PHY_HT_20_DS,      52000,  47000,  0x8b,  0x00,   11,   8,   10,       3,     3,    23,   54,   24,   54,  25730},
     /*   78 Mb */ {  TRUE20_2_3,     TRUE20_2_3,     TRUE,    TRUE20_N2_ALL_N1_T_D,WLAN_PHY_HT_20_DS,      78000,  70500,  0x8c,  0x00,   12,   8,   14,       3,     3,    24,   55,   25,   55,  38600},
     /*  104 Mb */ {  TRUE20_2_3,     TRUE20_2_3,     FALSE,   TRUE20_N2_ALL_N1_T_D,WLAN_PHY_HT_20_DS,     104000,  94000,  0x8d,  0x00,   13,   8,   20,       3,     2,    25,   56,   26,   56,  51470},
     /*  117 Mb */ {  TRUE20_2,       TRUE20_2,       FALSE,   TRUE20_N2_D_N1_D,    WLAN_PHY_HT_20_DS,     117000, 105200,  0x8e,  0x00,   14,   8,   23,       3,     2,    26,   57,   27,   57,  57910},
     /*  130 Mb */ {  TRUE20_2,       TRUE20_2,       TRUE,    TRUE20_N2_D_N1_D,    WLAN_PHY_HT_20_DS,     130000, 116100,  0x8f,  0x00,   15,   8,   25,       3,     2,    27,   58,   29,   59,  64340},
     /*144.4 Mb */ {  TRUE20_2,       TRUE20_2,       TRUE,    TRUE20_N2_D_N1_D,    WLAN_PHY_HT_20_DS_HGI, 144400, 128100,  0x8f,  0x00,   15,   8,   25,       3,     2,    27,   58,   29,   59,  71490},
     /* 19.5 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_20_TS,      19500,  17400,  0x90,  0x00,   16,   4,   25,       3,     3,    28,   60,   30,   60,   9630},
     /*   39 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_20_TS,      39000,  35100,  0x91,  0x00,   17,   6,   25,       3,     3,    29,   61,   31,   61,  19260},
     /* 58.5 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_20_TS,      58500,  52600,  0x92,  0x00,   18,   6,   25,       3,     3,    30,   62,   32,   62,  28890},
     /*   78 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_20_TS,      78000,  70400,  0x93,  0x00,   19,   8,   25,       3,     3,    31,   63,   33,   63,  38520},
     /*  117 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS,     117000, 104900,  0x94,  0x00,   20,   8,   25,       3,     3,    32,   64,   35,   65,  57790},
     /*  130 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS_HGI, 130000, 115800,  0x94,  0x00,   20,   8,   25,       3,     3,    32,   64,   35,   65,  64210},
     /*  156 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS,     156000, 137200,  0x95,  0x00,   21,   8,   25,       3,     3,    33,   66,   37,   67,  77060},
     /*173.3 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS_HGI, 173300, 151100,  0x95,  0x00,   21,   8,   25,       3,     3,    33,   66,   37,   67,  85620},
     /*175.5 Mb */ {  TRUE20_3,       TRUE20_3,       FALSE,   TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS,     175500, 152800,  0x96,  0x00,   22,   8,   25,       3,     3,    34,   68,   39,   69,  86690},
     /*  195 Mb */ {  TRUE20_3,       TRUE20_3,       FALSE,   TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS_HGI, 195000, 168400,  0x96,  0x00,   22,   8,   25,       3,     3,    34,   68,   39,   69,  96320},
     /*  195 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS,     195000, 168400,  0x97,  0x00,   23,   8,   25,       3,     3,    35,   70,   41,   71,  96320},
     /*216.7 Mb */ {  TRUE20_3,       TRUE20_3,       TRUE,    TRUE20_N2_T_N1_T,    WLAN_PHY_HT_20_TS_HGI, 216700, 185000,  0x97,  0x00,   23,   8,   25,       3,     3,    35,   70,   41,   71, 107030},
     /* 13.5 Mb */ {  TRUE40_1_2_3,   TRUE40_1_2_3,   TRUE,    TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      13500,  12100,  0x80,  0x00,    0,   8,    2,       3,     3,    12,   42,   42,   42,   6680},
     /* 27.0 Mb */ {  TRUE40_1_2_3,   TRUE40_1_2_3,   TRUE,    TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      27000,  24300,  0x81,  0x00,    1,   8,    4,       3,     3,    13,   43,   43,   43,  13370},
     /* 40.5 Mb */ {  TRUE40_1_2_3,   TRUE40_1_2_3,   FALSE,   TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      40500,  36500,  0x82,  0x00,    2,   8,    6,       3,     3,    14,   44,   44,   44,  20060},
     /*   54 Mb */ {  TRUE40_1_2,     TRUE40_1_2,     FALSE,   TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      54000,  48900,  0x83,  0x00,    3,   8,   10,       3,     3,    15,   45,   45,   45,  26750},
     /*   81 Mb */ {  TRUE40_1_2,     TRUE40_1_2,     TRUE,    TRUE40_N_1_2_ALL,    WLAN_PHY_HT_40_SS,      81000,  73300,  0x84,  0x00,    4,   8,   14,       3,     3,    16,   46,   46,   46,  40130},
     /*  108 Mb */ {  TRUE40_1,       TRUE40_1_2,     FALSE,   TRUE40_N2_F_N1_D_S,  WLAN_PHY_HT_40_SS,     108000,  97500,  0x85,  0x00,    5,   8,   20,       3,     1,    17,   47,   47,   47,  53510},
     /* 121.5Mb */ {  TRUE40_1,       TRUE40_1_2,     FALSE,   TRUE40_N2_F_N1_D_S,  WLAN_PHY_HT_40_SS,     121500, 109100,  0x86,  0x00,    6,   8,   23,       3,     1,    18,   48,   48,   48,  60200},
     /*  135 Mb */ {  TRUE40_1,       TRUE40_1,       TRUE,    TRUE40_N2_F_N1_S,    WLAN_PHY_HT_40_SS,     135000, 120400,  0x87,  0x00,    7,   8,   23,       3,     1,    19,   49,   50,   50,  66880},
     /*  150 Mb */ {  TRUE40_1,       TRUE40_1,       TRUE,    TRUE40_N2_F_N1_S,    WLAN_PHY_HT_40_SS_HGI, 150000, 133000,  0x87,  0x00,    7,   8,   25,       3,     1,    19,   49,   50,   50,  74320},
     /*   27 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    TRUE,    FALSE,               WLAN_PHY_HT_40_DS,      27000,  24100,  0x88,  0x00,    8,   8,    2,       3,     3,    20,   51,   51,   51,  13360},
     /*   54 Mb */ {  TRUE40_3,       TRUE40_3,       FALSE,   FALSE,               WLAN_PHY_HT_40_DS,      54000,  48700,  0x89,  0x00,    9,   8,    4,       3,     3,    21,   52,   52,   52,  26720},
     /*   81 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    FALSE,               WLAN_PHY_HT_40_DS,      81000,  73000,  0x8a,  0x00,   10,   8,    6,       3,     3,    22,   53,   53,   53,  40090},
     /*  108 Mb */ {  TRUE40_2_3,     TRUE40_3,       FALSE,   TRUE40_N2_ALL_N1_T,  WLAN_PHY_HT_40_DS,     108000,  97400,  0x8b,  0x00,   11,   8,   10,       3,     3,    23,   54,   54,   54,  53450},
     /*  162 Mb */ {  TRUE40_2_3,     TRUE40_2_3,     TRUE,    TRUE40_N2_ALL_N1_T_D,WLAN_PHY_HT_40_DS,     162000, 142400,  0x8c,  0x00,   12,   8,   14,       3,     3,    24,   55,   55,   55,  80180},
     /*  216 Mb */ {  TRUE40_2_3,     TRUE40_2_3,     FALSE,   TRUE40_N2_ALL_N1_T_D,WLAN_PHY_HT_40_DS,     216000, 185300,  0x8d,  0x00,   13,   8,   20,       3,     2,    25,   56,   56,   56, 106910},
     /*  243 Mb */ {  TRUE40_2,       TRUE40_2,       FALSE,   TRUE40_N2_D_N1_D,    WLAN_PHY_HT_40_DS,     243000, 206000,  0x8e,  0x00,   14,   8,   23,       3,     2,    26,   57,   57,   57, 120280},
     /*  270 Mb */ {  TRUE40_2,       TRUE40_2,       TRUE,    TRUE40_N2_D_N1_D,    WLAN_PHY_HT_40_DS,     270000, 225800,  0x8f,  0x00,   15,   8,   23,       3,     2,    27,   58,   59,   59, 133640},
     /*  300 Mb */ {  TRUE40_2,       TRUE40_2,       TRUE,    TRUE40_N2_D_N1_D,    WLAN_PHY_HT_40_DS_HGI, 300000, 247800,  0x8f,  0x00,   15,   8,   25,       3,     2,    27,   58,   59,   59, 148490},
     /* 40.5 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_40_TS,      40500,  36100,  0x90,  0x00,   16,   8,   25,       3,     3,    28,   60,   60,   60,  20000},
     /*   81 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_40_TS,      81000,  72900,  0x91,  0x00,   17,   8,   25,       3,     3,    29,   61,   61,   61,  40010},
     /*121.5 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_40_TS,     121500, 108300,  0x92,  0x00,   18,   8,   25,       3,     3,    30,   62,   62,   62,  60010},
     /*  162 Mb */ {  FALSE_1_2_3,    FALSE_1_2_3,    FALSE,   FALSE,               WLAN_PHY_HT_40_TS,     162000, 142000,  0x93,  0x00,   19,   8,   25,       3,     3,    31,   63,   63,   63,  80020},
     /*  243 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS,     243000, 205100,  0x94,  0x00,   20,   8,   25,       3,     3,    32,   64,   65,   65, 120030},
     /*  270 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS_HGI, 270000, 224700,  0x94,  0x00,   20,   8,   25,       3,     3,    32,   64,   65,   65, 133370},
     /*  324 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS,     324000, 263100,  0x95,  0x00,   21,   8,   25,       3,     3,    33,   66,   67,   67, 160050},
     /*  360 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS_HGI, 360000, 288000,  0x95,  0x00,   21,   8,   25,       3,     3,    33,   66,   67,   67, 177830},
     /*364.5 Mb */ {  TRUE40_3,       TRUE40_3,       FALSE,   TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS,     364500, 290700,  0x96,  0x00,   22,   8,   25,       3,     3,    34,   68,   69,   69, 180060},
     /*  405 Mb */ {  TRUE40_3,       TRUE40_3,       FALSE,   TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS_HGI, 405000, 317200,  0x96,  0x00,   22,   8,   25,       3,     3,    34,   68,   69,   69, 200060},
     /*  405 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS,     405000, 317200,  0x97,  0x00,   23,   8,   25,       3,     3,    35,   70,   71,   71, 200060},
     /*  450 Mb */ {  TRUE40_3,       TRUE40_3,       TRUE,    TRUE40_N2_T_N1_T,    WLAN_PHY_HT_40_TS_HGI, 450000, 346400,  0x97,  0x00,   23,   8,   25,       3,     3,    35,   70,   71,   71, 222290},
};

static RATE_TABLE_11N ar5416_11ngRateTable = {
    sizeof(ar5416_11ngRateTableInfo)/sizeof(RATE_INFO_11N),  /* number of rates */
    ar5416_11ngRateTableInfo,
    50,  /* probe interval */
    50,  /* rssi reduce interval */
    WLAN_RC_HT_FLAG,  /* Phy rates allowed initially */
};
#ifndef ATH_NO_5G_SUPPORT

static RATE_INFO_11N ar5416_11aRateTableInfo[] = {
     /*              Multi-strm      STBC            Valid for                                            short     dot11   ctrl  RssiAck  RssiAck  maxtx */
     /*              valid           valid           UAPSD  TxBF                     Kbps    uKbps   RC   Preamble  Rate    Rate  ValidMin DeltaMin chain */
     /*   6 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,   6000,   5400,  0x0b,  0x00, (0x80|12),  0,     2,       1,     3,    0, 0},
     /*   9 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,   9000,   7800,  0x0f,  0x00,        18,  0,     3,       1,     3,    1, 0},
     /*  12 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,  12000,  10000,  0x0a,  0x00, (0x80|24),  2,     4,       2,     3,    2, 0},
     /*  18 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,  18000,  13900,  0x0e,  0x00,        36,  2,     6,       2,     3,    3, 0},
     /*  24 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,  24000,  17300,  0x09,  0x00, (0x80|48),  4,    10,       3,     3,    4, 0},
     /*  36 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,  36000,  23000,  0x0d,  0x00,        72,  4,    14,       3,     3,    5, 0},
     /*  48 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,  48000,  27400,  0x08,  0x00,        96,  4,    19,       3,     1,    6, 0},
     /*  54 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,  54000,  29300,  0x0c,  0x00,       108,  4,    23,       3,     1,    7, 0},
};

static RATE_TABLE_11N ar5416_11aRateTable = {
    sizeof(ar5416_11aRateTableInfo)/sizeof(RATE_INFO_11N),  /* number of rates */
    ar5416_11aRateTableInfo,
    50,  /* probe interval */    
    50,  /* rssi reduce interval */
    0,   /* Phy rates allowed initially */
};

static RATE_INFO_11N ar5416_11aRateTableHalfInfo[] = {
     /*              Multi-strm      STBC            Valid for                                          short     dot11   ctrl  RssiAck  RssiAck  maxtx */
     /*              valid           valid           UAPSD  TxBF                    Kbps   uKbps   RC   Preamble  Rate    Rate  ValidMin DeltaMin chain */
     /*   6 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,   3000,  2700,  0x0b,  0x00,  (0x80|6),   0,     2,       1,     3,  0, 0},
     /*   9 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,   4500,  3900,  0x0f,  0x00,         9,   0,     3,       1,     3,  1, 0},
     /*  12 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,   6000,  5000,  0x0a,  0x00, (0x80|12),   2,     4,       2,     3,  2, 0},
     /*  18 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,   9000,  6950,  0x0e,  0x00,        18,   2,     6,       2,     3,  3, 0},
     /*  24 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,  12000,  8650,  0x09,  0x00, (0x80|24),   4,    10,       3,     3,  4, 0},
     /*  36 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,  18000, 11500,  0x0d,  0x00,        36,   4,    14,       3,     3,  5, 0},
     /*  48 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,  24000, 13700,  0x08,  0x00,        48,   4,    19,       3,     1,  6, 0},
     /*  54 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,  27000, 14650,  0x0c,  0x00,        54,   4,    23,       3,     1,  7, 0},
};

static RATE_TABLE_11N ar5416_11aRateTableHalf = {
    sizeof(ar5416_11aRateTableHalfInfo)/sizeof(RATE_INFO_11N),  /* number of rates */
    ar5416_11aRateTableHalfInfo,
    50,  /* probe interval */    
    50,  /* rssi reduce interval */
    0,   /* Phy rates allowed initially */
};

static RATE_INFO_11N ar5416_11aRateTableQuarterInfo[] = {
     /*              Multi-strm      STBC            Valid for                                          short     dot11   ctrl  RssiAck  RssiAck  maxtx */
     /*              valid           Valid           UAPSD  TxBF                    Kbps   uKbps   RC   Preamble  Rate    Rate  ValidMin DeltaMin chain */
     /*   6 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,   1500,  1350,  0x0b,  0x00,  (0x80|3),   0,     2,       1,     3,  0, 0},
     /*   9 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,   2250,  1950,  0x0f,  0x00,         4,   0,     3,       1,     3,  1, 0},
     /*  12 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,   3000,  2500,  0x0a,  0x00,  (0x80|6),   2,     4,       2,     3,  2, 0},
     /*  18 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,   4500,  3475,  0x0e,  0x00,         9,   2,     6,       2,     3,  3, 0},
     /*  24 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,   6000,  4325,  0x09,  0x00, (0x80|12),   4,    10,       3,     3,  4, 0},
     /*  36 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,   9000,  5750,  0x0d,  0x00,        18,   4,    14,       3,     3,  5, 0},
     /*  48 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_OFDM,  12000,  6850,  0x08,  0x00,        24,   4,    19,       3,     1,  6, 0},
     /*  54 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_OFDM,  13500,  7325,  0x0c,  0x00,        27,   4,    23,       3,     1,  7, 0},
};

static RATE_TABLE_11N ar5416_11aRateTableQuarter = {
    sizeof(ar5416_11aRateTableQuarterInfo)/sizeof(RATE_INFO_11N),  /* number of rates */
    ar5416_11aRateTableQuarterInfo,
    50,  /* probe interval */    
    50,  /* rssi reduce interval */
    0,   /* Phy rates allowed initially */
};

#endif /* #ifndef ATH_NO_5G_SUPPORT */

static RATE_INFO_11N ar5416_TurboRateTableInfo[] = {
     /*              Multi-strm      STBC            Valid for                                          short     dot11   ctrl  RssiAck  RssiAck  maxtx */
     /*              valid           Valid           UAPSD  TxBF                    Kbps   uKbps   RC   Preamble  Rate    Rate  ValidMin DeltaMin chain */
     /*   6 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_TURBO,  6000,  5400,  0x0b,  0x00, (0x80|12),   0,     2,       1,     3,  0, 0},
     /*   9 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_TURBO,  9000,  7800,  0x0f,  0x00,        18,   0,     4,       1,     3,  1, 0},
     /*  12 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_TURBO, 12000, 10000,  0x0a,  0x00, (0x80|24),   2,     7,       2,     3,  2, 0},
     /*  18 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_TURBO, 18000, 13900,  0x0e,  0x00,        36,   2,     9,       2,     3,  3, 0},
     /*  24 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_TURBO, 24000, 17300,  0x09,  0x00, (0x80|48),   4,    14,       3,     3,  4, 0},
     /*  36 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_TURBO, 36000, 23000,  0x0d,  0x00,        72,   4,    17,       3,     3,  5, 0},
     /*  48 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_TURBO, 48000, 27400,  0x08,  0x00,        96,   4,    22,       3,     1,  6, 0},
     /*  54 Mb */ {  TRUE_1_2_3,     FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_TURBO, 54000, 29300,  0x0c,  0x00,       108,   4,    26,       3,     1,  7, 0},
};

static RATE_TABLE_11N ar5416_TurboRateTable = {
    sizeof(ar5416_TurboRateTableInfo)/sizeof(RATE_INFO_11N),  /* number of rates */
    ar5416_TurboRateTableInfo,
    50, /* probe interval */    
    50,  /* rssi reduce interval */
    0,   /* Phy rates allowed initially */
};

/* Venice TODO: roundUpRate() is broken when the rate table does not represent rates
 * in increasing order  e.g.  5.5, 11, 6, 9.
 * An average rate of 6 Mbps will currently map to 11 Mbps.
 */
static RATE_INFO_11N ar5416_11gRateTableInfo[] = {
     /*              Multi-strm       STBC            Valid for                                         short     dot11  ctrl  RssiAck  RssiAck  maxtx */
     /*              valid            Valid           UAPSD TxBF                    Kbps   uKbps   RC   Preamble  Rate   Rate  ValidMin DeltaMin chain */
     /*   1 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    TRUE, FALSE,  WLAN_PHY_CCK,   1000,   900,  0x1b,  0x00,      2,     0,     0,       1,     3,   0, 0},
     /*   2 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    FALSE,FALSE,  WLAN_PHY_CCK,   2000,  1900,  0x1a,  0x04,      4,     1,     1,       1,     3,   1, 0},
     /* 5.5 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    FALSE,FALSE,  WLAN_PHY_CCK,   5500,  4900,  0x19,  0x04,     11,     2,     2,       2,     3,   2, 0},
     /*  11 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    TRUE, FALSE,  WLAN_PHY_CCK,  11000,  8100,  0x18,  0x04,     22,     3,     3,       2,     3,   3, 0},
     /*   6 Mb */ {  FALSE_1_2_3,     FALSE_1_2_3,    FALSE,FALSE,  WLAN_PHY_OFDM,  6000,  5400,  0x0b,  0x00,     12,     4,     2,       1,     3,   4, 0},
     /*   9 Mb */ {  FALSE_1_2_3,     FALSE_1_2_3,    FALSE,FALSE,  WLAN_PHY_OFDM,  9000,  7800,  0x0f,  0x00,     18,     4,     3,       1,     3,   5, 0},
     /*  12 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    FALSE,FALSE,  WLAN_PHY_OFDM, 12000, 10000,  0x0a,  0x00,     24,     6,     4,       1,     3,   6, 0},
     /*  18 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    FALSE,FALSE,  WLAN_PHY_OFDM, 18000, 13900,  0x0e,  0x00,     36,     6,     6,       2,     3,   7, 0},
     /*  24 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    TRUE, FALSE,  WLAN_PHY_OFDM, 24000, 17300,  0x09,  0x00,     48,     8,    10,       3,     3,   8, 0},
     /*  36 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    FALSE,FALSE,  WLAN_PHY_OFDM, 36000, 23000,  0x0d,  0x00,     72,     8,    14,       3,     3,   9, 0},
     /*  48 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    FALSE,FALSE,  WLAN_PHY_OFDM, 48000, 27400,  0x08,  0x00,     96,     8,    19,       3,     1,  10, 0},
     /*  54 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    TRUE, FALSE,  WLAN_PHY_OFDM, 54000, 29300,  0x0c,  0x00,    108,     8,    23,       3,     1,  11, 0},
};

static RATE_TABLE_11N ar5416_11gRateTable = {
    sizeof(ar5416_11gRateTableInfo)/sizeof(RATE_INFO_11N),  /* number of rates */
    ar5416_11gRateTableInfo,
    50,  /* probe interval */    
    50,  /* rssi reduce interval */    
    0,   /* Phy rates allowed initially */    
};

static RATE_INFO_11N ar5416_11bRateTableInfo[] = {
     /*              Multi-strm       STBC            Valid for                                         short     dot11   ctrl  RssiAck  RssiAck  maxtx */
     /*              valid            Valid           UAPSD  TxBF                  Kbps   uKbps   RC   Preamble   Rate    Rate  ValidMin DeltaMin chain */
     /*   1 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_CCK,  1000,   900,  0x1b,   0x00, (0x80| 2),   0,    0,       1,     3,   0, 0},
     /*   2 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_CCK,  2000,  1800,  0x1a,   0x04, (0x80| 4),   1,    1,       1,     3,   1, 0},
     /* 5.5 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    FALSE, FALSE,  WLAN_PHY_CCK,  5500,  4300,  0x19,   0x04, (0x80|11),   1,    2,       2,     3,   2, 0},
     /*  11 Mb */ {  TRUE_1_2_3,      FALSE_1_2_3,    TRUE,  FALSE,  WLAN_PHY_CCK, 11000,  7100,  0x18,   0x04, (0x80|22),   1,    4,     100,     3,   3, 0},
};

static RATE_TABLE_11N ar5416_11bRateTable = {
    sizeof(ar5416_11bRateTableInfo)/sizeof(RATE_INFO_11N),  /* number of rates */
    ar5416_11bRateTableInfo,
    100, /* probe interval */    
    100, /* rssi reduce interval */    
    0,   /* Phy rates allowed initially */
};

void
ar5416SetupRateTables(void)
{
}

void
ar5416AttachRateTables(struct atheros_softc *sc)
{
    /*
     * Attach device specific rate tables; for ar5212.
     * 11a static turbo and 11g static turbo share the same table.
     * Dynamic turbo uses combined rate table.
     */
    sc->hwRateTable[WIRELESS_MODE_11b]           = &ar5416_11bRateTable;
    sc->hwRateTable[WIRELESS_MODE_11g]           = &ar5416_11gRateTable;
    sc->hwRateTable[WIRELESS_MODE_108g]          = &ar5416_TurboRateTable;
#ifndef ATH_NO_5G_SUPPORT
    sc->hwRateTable[WIRELESS_MODE_11a]           = &ar5416_11aRateTable;
    sc->hwRateTable[WIRELESS_MODE_108a]          = &ar5416_TurboRateTable;
#endif

#ifndef ATH_NO_5G_SUPPORT
    sc->hwRateTable[WIRELESS_MODE_11NA_HT20]        = &ar5416_11naRateTable;
    sc->hwRateTable[WIRELESS_MODE_11NA_HT40PLUS]       = &ar5416_11naRateTable;
    sc->hwRateTable[WIRELESS_MODE_11NA_HT40MINUS]       = &ar5416_11naRateTable;
#endif
    sc->hwRateTable[WIRELESS_MODE_11NG_HT20]           = &ar5416_11ngRateTable;
    sc->hwRateTable[WIRELESS_MODE_11NG_HT40PLUS]       = &ar5416_11ngRateTable;
    sc->hwRateTable[WIRELESS_MODE_11NG_HT40MINUS]       = &ar5416_11ngRateTable;
}

#ifndef ATH_NO_5G_SUPPORT
void
ar5416SetQuarterRateTable(struct atheros_softc *sc)
{
    sc->hwRateTable[WIRELESS_MODE_11a] = &ar5416_11aRateTableQuarter;
    return;
}

void
ar5416SetHalfRateTable(struct atheros_softc *sc)
{
    sc->hwRateTable[WIRELESS_MODE_11a] = &ar5416_11aRateTableHalf;
    return;
}

void
ar5416SetFullRateTable(struct atheros_softc *sc)
{
    sc->hwRateTable[WIRELESS_MODE_11a]   = &ar5416_11aRateTable;
    return;
}
#endif /* #ifndef ATH_NO_5G_SUPPORT */

/*   * This routine is called to map the baseIndex to the rate in the RATE_TABLE     
 */ 
u_int32_t
ar5416_rate_maprix(struct atheros_softc *asc, WIRELESS_MODE curmode,  u_int8_t baseIndex,u_int8_t flags, int isratecode)
{
        u_int8_t                rix;
        u_int32_t               rate;
        const RATE_TABLE_11N *pRateTable = (const RATE_TABLE_11N *)asc->hwRateTable[curmode];
 
        if ((flags & ATH_RC_CW40_FLAG) && (flags & ATH_RC_SGI_FLAG)) {
        rix = pRateTable->info[baseIndex].htIndex;
        } else if (flags & ATH_RC_SGI_FLAG) {
        rix = pRateTable->info[baseIndex].sgiIndex;
        } else if (flags & ATH_RC_CW40_FLAG) {
        rix = pRateTable->info[baseIndex].cw40Index;
        } else {
                rix = baseIndex;
        }
        if (isratecode)         
                rate = pRateTable->info[rix].rateCode;
        else
                rate = pRateTable->info[rix].rateKbps/1000;
        return rate;
}       




