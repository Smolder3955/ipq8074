/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2007, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */


 /* Contains definitions for Owl */


#ifndef _ATH_AR5416_DESC_H_
#define _ATH_AR5416_DESC_H_

#ifdef WIN32
#pragma pack(push, ar5416desc, 1)
#endif

/* Owl descriptor. */

/*
 * Do not attempt to use the __packed attribute to 
 * this desc data structure to save more CPU cycles
 * because gcc tend to put extra code for protection
 * with this attribute. 
 * Be EXTRA care that all fields should be 32 bits
 */

struct ar5416_desc {
    u_int32_t   ds_link;    /* link pointer */
    u_int32_t   ds_data;    /* data buffer pointer */
    u_int32_t   ds_ctl0;    /* DMA control 0 */
    u_int32_t   ds_ctl1;    /* DMA control 1 */
    union {
        struct { /* tx desc has 8 control words(for owl1.0)/12 
                    control words(for owl2.0) + 10 status words */
            struct {
			u_int32_t   ctl2;
            u_int32_t   ctl3;
            u_int32_t   ctl4;
            u_int32_t   ctl5;
            u_int32_t   ctl6;
            u_int32_t   ctl7;
            u_int32_t   ctl8;
            u_int32_t   ctl9;
            u_int32_t	ctl10;
            u_int32_t	ctl11;
			} tx_ctl;
			struct {
            u_int32_t   status0;
            u_int32_t   status1;
            u_int32_t   status2;
            u_int32_t   status3;
            u_int32_t   status4;
            u_int32_t   status5;
            u_int32_t   status6;
            u_int32_t   status7;
            u_int32_t   status8;
            u_int32_t   status9;
			} tx_status;
        } tx;
        struct { /* rx desc has 2 control words + 9 status words */
            u_int32_t   status0;
            u_int32_t   status1;
            u_int32_t   status2;
            u_int32_t   status3;
            u_int32_t   status4;
            u_int32_t   status5;
            u_int32_t   status6;
            u_int32_t   status7;
            u_int32_t   status8;
        } rx;
    } u;
}; 

#define AR5416DESC(_ds)         ((struct ar5416_desc *)(_ds))
#define AR5416DESC_CONST(_ds)   ((const struct ar5416_desc *)(_ds))

#define ds_ctl2     u.tx.tx_ctl.ctl2
#define ds_ctl3     u.tx.tx_ctl.ctl3
#define ds_ctl4     u.tx.tx_ctl.ctl4
#define ds_ctl5     u.tx.tx_ctl.ctl5
#define ds_ctl6     u.tx.tx_ctl.ctl6
#define ds_ctl7     u.tx.tx_ctl.ctl7
#define ds_ctl8     u.tx.tx_ctl.ctl8
#define ds_ctl9     u.tx.tx_ctl.ctl9
#define ds_ctl10    u.tx.tx_ctl.ctl10
#define ds_ctl11    u.tx.tx_ctl.ctl11

#define ds_txstatus0    u.tx.tx_status.status0
#define ds_txstatus1    u.tx.tx_status.status1
#define ds_txstatus2    u.tx.tx_status.status2
#define ds_txstatus3    u.tx.tx_status.status3
#define ds_txstatus4    u.tx.tx_status.status4
#define ds_txstatus5    u.tx.tx_status.status5
#define ds_txstatus6    u.tx.tx_status.status6
#define ds_txstatus7    u.tx.tx_status.status7
#define ds_txstatus8    u.tx.tx_status.status8
#define ds_txstatus9    u.tx.tx_status.status9

#define ds_rxstatus0    u.rx.status0
#define ds_rxstatus1    u.rx.status1
#define ds_rxstatus2    u.rx.status2
#define ds_rxstatus3    u.rx.status3
#define ds_rxstatus4    u.rx.status4
#define ds_rxstatus5    u.rx.status5
#define ds_rxstatus6    u.rx.status6
#define ds_rxstatus7    u.rx.status7
#define ds_rxstatus8    u.rx.status8

/***********
 * TX Desc *
 ***********/

/* ds_ctl0 */
#define AR_FrameLen         0x00000fff
#define AR_VirtMoreFrag     0x00001000
#define AR_LowRxChain       0x00004000
#define AR_TxCtlRsvd00      0x0000a000
#define AR_XmitPower0       0x003f0000
#define AR_XmitPower0_S     16
#define AR_RTSEnable        0x00400000
#define AR_VEOL             0x00800000
#define AR_ClrDestMask      0x01000000
#define AR_TxCtlRsvd01      0x1e000000
#define AR_TxIntrReq        0x20000000
#define AR_DestIdxValid     0x40000000
#define AR_CTSEnable        0x80000000

/* ds_ctl1 */
#define AR_BufLen           0x00000fff
#define AR_TxMore           0x00001000
#define AR_DestIdx          0x000fe000
#define AR_DestIdx_S        13
#define AR_FrameType        0x00f00000
#define AR_FrameType_S      20
#define AR_NoAck            0x01000000
#define AR_InsertTS         0x02000000
#define AR_CorruptFCS       0x04000000
#define AR_ExtOnly          0x08000000
#define AR_ExtAndCtl        0x10000000
#define AR_MoreAggr         0x20000000
#define AR_IsAggr           0x40000000
#define AR_MoreRifs	    0x80000000

/* ds_ctl2 */
#define AR_BurstDur         0x00007fff
#define AR_BurstDur_S       0
#define AR_DurUpdateEna     0x00008000
#define AR_XmitDataTries0   0x000f0000
#define AR_XmitDataTries0_S 16
#define AR_XmitDataTries1   0x00f00000
#define AR_XmitDataTries1_S 20
#define AR_XmitDataTries2   0x0f000000
#define AR_XmitDataTries2_S 24
#define AR_XmitDataTries3   0xf0000000
#define AR_XmitDataTries3_S 28

/* ds_ctl3 */
#define AR_XmitRate0        0x000000ff
#define AR_XmitRate0_S      0
#define AR_XmitRate1        0x0000ff00
#define AR_XmitRate1_S      8
#define AR_XmitRate2        0x00ff0000
#define AR_XmitRate2_S      16
#define AR_XmitRate3        0xff000000
#define AR_XmitRate3_S      24

/* ds_ctl4 */
#define AR_PacketDur0       0x00007fff
#define AR_PacketDur0_S     0
#define AR_RTSCTSQual0      0x00008000
#define AR_PacketDur1       0x7fff0000
#define AR_PacketDur1_S     16
#define AR_RTSCTSQual1      0x80000000

/* ds_ctl5 */
#define AR_PacketDur2       0x00007fff
#define AR_PacketDur2_S     0
#define AR_RTSCTSQual2      0x00008000
#define AR_PacketDur3       0x7fff0000
#define AR_PacketDur3_S     16
#define AR_RTSCTSQual3      0x80000000

/* ds_ctl6 */
#define AR_AggrLen          0x0000ffff
#define AR_AggrLen_S        0
#define AR_TxCtlRsvd60      0x00030000
#define AR_PadDelim         0x03fc0000
#define AR_PadDelim_S       18
#define AR_EncrType         0x1c000000
#define AR_EncrType_S       26
#define AR_TxCtlRsvd61      0xf0000000

/* ds_ctl 7 */
#define AR_2040_0           0x00000001
#define AR_GI0              0x00000002
#define AR_ChainSel0        0x0000001c
#define AR_ChainSel0_S      2
#define AR_2040_1           0x00000020
#define AR_GI1              0x00000040
#define AR_ChainSel1        0x00000380
#define AR_ChainSel1_S      7
#define AR_2040_2           0x00000400
#define AR_GI2              0x00000800
#define AR_ChainSel2        0x00007000
#define AR_ChainSel2_S      12
#define AR_2040_3           0x00008000
#define AR_GI3              0x00010000
#define AR_ChainSel3        0x000e0000
#define AR_ChainSel3_S      17
#define AR_RTSCTSRate       0x0ff00000
#define AR_RTSCTSRate_S     20
#define AR_STBC0            0x10000000
#define AR_STBC1            0x20000000
#define AR_STBC2            0x40000000
#define AR_STBC3            0x80000000

/* ds_ctl 8 */
#define AR_TxCtlRsvd80      0xffffffff

/* ds_ctl 9 */
#define AR_TxCtlRsvd90      0x00ffffff
#define AR_XmitPower1       0x3f000000
#define AR_XmitPower1_S     24
#define AR_TxCtlRsvd91      0xc0000000

/* ds_ctl 10 */
#define AR_TxCtlRsvd100     0x00ffffff
#define AR_XmitPower2       0x3f000000
#define AR_XmitPower2_S     24
#define AR_TxCtlRsvd101     0xc0000000

/* ds_ctl 11 */
#define AR_TxCtlRsvd110     0x00ffffff
#define AR_XmitPower3       0x3f000000
#define AR_XmitPower3_S     24
#define AR_TxCtlRsvd111     0xc0000000

/*************
 * TX Status *
 *************/

/* ds_status0 */
#define AR_TxRSSIAnt00      0x000000ff
#define AR_TxRSSIAnt00_S    0
#define AR_TxRSSIAnt01      0x0000ff00
#define AR_TxRSSIAnt01_S    8
#define AR_TxRSSIAnt02      0x00ff0000
#define AR_TxRSSIAnt02_S    16
#define AR_TxStatusRsvd00   0x3f000000
#define AR_TxBaStatus       0x40000000
#define AR_TxStatusRsvd01   0x80000000

/* ds_status1 */
#define AR_FrmXmitOK            0x00000001
#define AR_ExcessiveRetries     0x00000002
#define AR_FIFOUnderrun         0x00000004
#define AR_Filtered             0x00000008
#define AR_RTSFailCnt           0x000000f0
#define AR_RTSFailCnt_S         4
#define AR_DataFailCnt          0x00000f00
#define AR_DataFailCnt_S        8
#define AR_VirtRetryCnt         0x0000f000
#define AR_VirtRetryCnt_S       12
#define AR_TxDelimUnderrun      0x00010000
#define AR_TxDataUnderrun       0x00020000
#define AR_DescCfgErr           0x00040000
#define AR_TxTimerExpired       0x00080000
#define AR_TxStatusRsvd10       0xfff00000

/* ds_status2 */
#define AR_SendTimestamp    ds_txstatus2

/* ds_status3 */
#define AR_BaBitmapLow      ds_txstatus3

/* ds_status4 */
#define AR_BaBitmapHigh     ds_txstatus4

/* ds_status5 */
#define AR_TxRSSIAnt10      0x000000ff
#define AR_TxRSSIAnt10_S    0
#define AR_TxRSSIAnt11      0x0000ff00
#define AR_TxRSSIAnt11_S    8
#define AR_TxRSSIAnt12      0x00ff0000
#define AR_TxRSSIAnt12_S    16
#define AR_TxRSSICombined   0xff000000
#define AR_TxRSSICombined_S 24

/* ds_status6 */
#define AR_TxEVM0           ds_txstatus5

/* ds_status7 */
#define AR_TxEVM1           ds_txstatus6

/* ds_status8 */
#define AR_TxEVM2           ds_txstatus7

/* ds_status9 */
#define AR_TxDone           0x00000001
#define AR_SeqNum           0x00001ffe
#define AR_SeqNum_S         1
#define AR_TxStatusRsvd80   0x0001e000
#define AR_TxOpExceeded     0x00020000
#define AR_TxStatusRsvd81   0x001c0000
#define AR_FinalTxIdx       0x00600000
#define AR_FinalTxIdx_S     21
#define AR_TxStatusRsvd82   0x01800000
#define AR_PowerMgmt        0x02000000
#define AR_TxStatusRsvd83   0xfc000000

/***********
 * RX Desc *
 ***********/

/* ds_ctl0 */
#define AR_RxCTLRsvd00  0xffffffff

/* ds_ctl1 */
#define AR_BufLen       0x00000fff
#define AR_RxCtlRsvd00  0x00001000
#define AR_RxIntrReq    0x00002000
#define AR_RxCtlRsvd01  0xffffc000

/*************
 * Rx Status *
 *************/

/* ds_status0 */
#define AR_RxRSSIAnt00      0x000000ff
#define AR_RxRSSIAnt00_S    0
#define AR_RxRSSIAnt01      0x0000ff00
#define AR_RxRSSIAnt01_S    8
#define AR_RxRSSIAnt02      0x00ff0000
#define AR_RxRSSIAnt02_S    16
#define AR_RxRate           0xff000000		/* use RXSTATUS_RATE to abstract Owl 1.0/2.0 differences */
#define AR_RxRate_S         24
#define AR_RxStatusRsvd00   0xff000000

/* ds_status1 */
#define AR_DataLen          0x00000fff
#define AR_RxMore           0x00001000
#define AR_NumDelim         0x003fc000
#define AR_NumDelim_S       14
#define AR_RxStatusRsvd10   0xff800000

/* ds_status2 */
#define AR_RcvTimestamp     ds_rxstatus2

/* ds_status3 */
#define AR_GI               0x00000001
#define AR_2040             0x00000002
#define AR_Parallel40       0x00000004		/* use RXSTATUS_DUPLICATE to abstract Owl 1.0/2.0 differences */
#define AR_Parallel40_S     2
#define AR_STBC             0x00000008
#define AR_RxStatusRsvd30   0x000000f0
#define AR_RxAntenna	    0xffffff00
#define AR_RxAntenna_S	    8

/* ds_status4 */
#define AR_RxRSSIAnt10            0x000000ff
#define AR_RxRSSIAnt10_S          0
#define AR_RxRSSIAnt11            0x0000ff00
#define AR_RxRSSIAnt11_S          8
#define AR_RxRSSIAnt12            0x00ff0000
#define AR_RxRSSIAnt12_S          16
#define AR_RxRSSICombined         0xff000000
#define AR_RxRSSICombined_S       24

/* ds_status5 */
#define AR_RxEVM0           ds_rxstatus5

/* ds_status6 */
#define AR_RxEVM1           ds_rxstatus6

/* ds_status7 */
#define AR_RxEVM2           ds_rxstatus7

/* ds_status8 */
#define AR_RxDone           0x00000001
#define AR_RxFrameOK        0x00000002
#define AR_CRCErr           0x00000004
#define AR_DecryptCRCErr    0x00000008
#define AR_PHYErr           0x00000010
#define AR_MichaelErr       0x00000020
#define AR_PreDelimCRCErr   0x00000040
#define AR_RxStatusRsvd70   0x00000080
#define AR_RxKeyIdxValid    0x00000100
#define AR_KeyIdx           0x0000fe00
#define AR_KeyIdx_S         9
#define AR_PHYErrCode       0x0000ff00
#define AR_PHYErrCode_S     8
#define AR_RxMoreAggr       0x00010000
#define AR_RxAggr           0x00020000
#define AR_PostDelimCRCErr  0x00040000
#define AR_RxStatusRsvd71   0x0ff80000
#define AR_HiRxChain        0x10000000  
#define AR_RxFirstAggr      0x20000000  
#define AR_DecryptBusyErr   0x40000000
#define AR_KeyMiss          0x80000000

#define AR5416_CONTTXMODE(ah,args,argsize )     ar5416_ContTxMode(ah, args, argsize )
#define TXCTL_OFFSET(ah)      2
#define TXCTL_NUMWORDS(ah)    (AR_SREV_5416_V20_OR_LATER(ah) ? 12 : 8)
#define TXSTATUS_OFFSET(ah)   (AR_SREV_5416_V20_OR_LATER(ah) ? 14 : 10)
#define TXSTATUS_NUMWORDS(ah) 10

#define RXCTL_OFFSET(ah)      3
#define RXCTL_NUMWORDS(ah)    1
#define RXSTATUS_OFFSET(ah)   4
#define RXSTATUS_NUMWORDS(ah) 9
#define RXSTATUS_RATE(ah, ads)  (AR_SREV_5416_V20_OR_LATER(ah) ?  		\
                                            MS(ads->ds_rxstatus0, AR_RxRate) :		\
					    (ads->ds_rxstatus3 >> 2) & 0xFF)
#define RXSTATUS_DUPLICATE(ah, ads)  (AR_SREV_5416_V20_OR_LATER(ah) ?	\
                                            MS(ads->ds_rxstatus3, AR_Parallel40) :      \
					    (ads->ds_rxstatus3 >> 10) & 0x1)
#define VALID_KEY_TYPES \
        ((1 << HAL_KEY_TYPE_CLEAR) | (1 << HAL_KEY_TYPE_WEP)|\
         (1 << HAL_KEY_TYPE_AES)   | (1 << HAL_KEY_TYPE_TKIP))
#define isValidKeyType(_t)      ((1 << (_t)) & VALID_KEY_TYPES)

#define set11nTries(_series, _index) \
        (SM((_series)[_index].Tries, AR_XmitDataTries##_index))

#define set11nRate(_series, _index) \
        (SM((_series)[_index].Rate, AR_XmitRate##_index))

#define set11nPktDurRTSCTS(_series, _index) \
        (SM((_series)[_index].PktDuration, AR_PacketDur##_index) |\
         ((_series)[_index].RateFlags & HAL_RATESERIES_RTS_CTS   ?\
         AR_RTSCTSQual##_index : 0))

#define set11nRateFlags(_series, _index) \
        ((_series)[_index].RateFlags & HAL_RATESERIES_2040 ? AR_2040_##_index : 0) \
        |((_series)[_index].RateFlags & HAL_RATESERIES_HALFGI ? AR_GI##_index : 0) \
        |((_series)[_index].RateFlags & HAL_RATESERIES_STBC ? AR_STBC##_index : 0) \
        |SM((_series)[_index].ch_sel, AR_ChainSel##_index)

#define set11nTxPower(_index, _txpower) \
        SM(_txpower, AR_XmitPower##_index)

/*
 * Descriptor Access Functions
 */

#define VALID_PKT_TYPES \
        ((1<<HAL_PKT_TYPE_NORMAL)|(1<<HAL_PKT_TYPE_ATIM)|\
         (1<<HAL_PKT_TYPE_PSPOLL)|(1<<HAL_PKT_TYPE_PROBE_RESP)|\
         (1<<HAL_PKT_TYPE_BEACON))
#define isValidPktType(_t)      ((1<<(_t)) & VALID_PKT_TYPES)
#define VALID_TX_RATES \
        ((1<<0x0b)|(1<<0x0f)|(1<<0x0a)|(1<<0x0e)|(1<<0x09)|(1<<0x0d)|\
         (1<<0x08)|(1<<0x0c)|(1<<0x1b)|(1<<0x1a)|(1<<0x1e)|(1<<0x19)|\
         (1<<0x1d)|(1<<0x18)|(1<<0x1c))
#define isValidTxRate(_r)       ((1<<(_r)) & VALID_TX_RATES)

#ifdef WIN32
#pragma pack(pop, ar5416desc)
#endif

#endif



