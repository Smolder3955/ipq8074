/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2011, Atheros Communications Inc.
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
 * Provide format conversion tools between eeprom format and user format
 * TODO: currently only support AR98XX devices.
 */

#include "ol_if_athvar.h"
#include "ol_if_eeprom.h"
#include "target_type.h"
#include "init_deinit_lmac.h"

A_UINT8 *pQc9kEepromArea = NULL;

/* For receiving and reordering fragments from target */
QC98XX_EEPROM_RATETBL eep_ratepwr_tbl;

#define EEPROM_BASE_POINTER     ((QC98XX_EEPROM_RATETBL *)(pQc9kEepromArea))

// From user perspective, target powers for each pier defined in caltgtfreqvht40/20/80 as follow:
// 24 values represent the target power in dBm for the following data rate
// MCS0, MCS1_2, MCS3_4, MCS5, MCS6, MCS7, MCS8, MCS9,
// MCS10, MCS11_12, MCS13_14, MCS15, MCS16, MCS17, MCS18, MCS19,
// MCS20, MCS21_22, MCS23_24, MCS25, MCS26, MCS27, MCS28, MCS29,
//
// From programming perspective, the target powers for each pier are stored as follow:
// Each stream will have a base target power
// 18 values (4-bit each) represent the target power delta against the based for the following data rate:
// MCS0_10_20, MCS1_2_11_12_21_22, MCS3_4_13_14_23_24,
// MCS5, MCS6, MCS7, MCS8, MCS9,
// MCS15, MCS16, MCS17, MCS18, MCS19,
// MCS25, MCS26, MCS27, MCS28, MCS29,
//      Delta of MCS0_10_20 is stored in tPow2xDelta[3:0]; delta of MCS1_2_11_12_21_22 is stored in tPow2xDelta[7:4], and so on
// 11/19/2012 - The delta is extended to 5 bits. The extended bits are defined in extTPow2xDelta2G and extTPow2xDelta5G
// The calculation to get the 5th bit of a given [bandwith, pier, rateIndex] is:
// bitthInArray = ( (MAX_PIERS * bandwith) + pier) * MAX_RATE_INDEX + rateIndex;
// fifthBit = (extTPow2xDelta[bitthInArray >> 3] >> (bitthInArray % 8)) & 1;
//
// where: MAX_PIERS = 3 for 2GHz and 6 for 5GHz; MAX_RATE_INDEX = 18
//               bandwith: 0 for VHT20, 1 for VHT40, 2 for VHT80
//               pier: 0..2 for 2GHz; 0..5 for 5GHz.

// This table is used to map the user rate index to the program rate index
A_UINT16 VhtRateIndexToRateGroupTable[] =
{
        WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20,                         //VHT_TARGET_RATE_0,
        WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22,         //VHT_TARGET_RATE_1_2,
        WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24,         //VHT_TARGET_RATE_3_4,
        WHAL_VHT_TARGET_POWER_RATES_MCS5,                                       //VHT_TARGET_RATE_5,
        WHAL_VHT_TARGET_POWER_RATES_MCS6,                                       //VHT_TARGET_RATE_6,
        WHAL_VHT_TARGET_POWER_RATES_MCS7,                                       //VHT_TARGET_RATE_7,
        WHAL_VHT_TARGET_POWER_RATES_MCS8,                                       //VHT_TARGET_RATE_8,
        WHAL_VHT_TARGET_POWER_RATES_MCS9,                                       //VHT_TARGET_RATE_9,
        WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20,                         //VHT_TARGET_RATE_10,
        WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22,         //VHT_TARGET_RATE_11_12,
        WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24,         //VHT_TARGET_RATE_13_14,
        WHAL_VHT_TARGET_POWER_RATES_MCS15,                                      //VHT_TARGET_RATE_15,
        WHAL_VHT_TARGET_POWER_RATES_MCS16,                                      //VHT_TARGET_RATE_16,
        WHAL_VHT_TARGET_POWER_RATES_MCS17,                                      //VHT_TARGET_RATE_17,
        WHAL_VHT_TARGET_POWER_RATES_MCS18,                                      //VHT_TARGET_RATE_18,
        WHAL_VHT_TARGET_POWER_RATES_MCS19,                                      //VHT_TARGET_RATE_19,
        WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20,                         //VHT_TARGET_RATE_20,
        WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22,         //VHT_TARGET_RATE_21_22,
        WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24,         //VHT_TARGET_RATE_23_24,
        WHAL_VHT_TARGET_POWER_RATES_MCS25,                                      //VHT_TARGET_RATE_25,
        WHAL_VHT_TARGET_POWER_RATES_MCS26,                                      //VHT_TARGET_RATE_26,
        WHAL_VHT_TARGET_POWER_RATES_MCS27,                                      //VHT_TARGET_RATE_27,
        WHAL_VHT_TARGET_POWER_RATES_MCS28,                                      //VHT_TARGET_RATE_28,
        WHAL_VHT_TARGET_POWER_RATES_MCS29,                                      //VHT_TARGET_RATE_29,
};

A_BOOL GetTgtPowerExtBitInfo (A_UINT16 pierth, A_UINT8 vhtIdx, A_UINT16 rateIndex, A_BOOL is2GHz,
                              A_UINT16 *bitthInArray, A_UINT8 *bitthInByte, A_UINT8 *bytethInArray)
{
    A_UINT8 maxPier;

    if (is2GHz)
    {
        maxPier = (vhtIdx == CAL_TARGET_POWER_VHT20_INDEX) ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11G_40_TARGET_POWER_CHANS;
    }
    else //5GHz
    {
        maxPier = vhtIdx == CAL_TARGET_POWER_VHT20_INDEX ? WHAL_NUM_11A_20_TARGET_POWER_CHANS :
                    (vhtIdx == CAL_TARGET_POWER_VHT40_INDEX ? WHAL_NUM_11A_40_TARGET_POWER_CHANS : WHAL_NUM_11A_80_TARGET_POWER_CHANS);
    }
    *bitthInArray = ((maxPier * vhtIdx) + pierth) * WHAL_NUM_VHT_TARGET_POWER_RATES + rateIndex;
    *bitthInByte = *bitthInArray % 8;
    *bytethInArray = *bitthInArray >> 3;
    if (*bytethInArray >= (is2GHz ? QC98XX_EXT_TARGET_POWER_SIZE_2G : QC98XX_EXT_TARGET_POWER_SIZE_5G))
    {
        return  FALSE;
    }
    return TRUE;
}

A_UINT8 GetTgtPowerExtByteIndex (A_UINT16 pierth, A_UINT8 vhtIdx, A_UINT16 userTargetPowerRateIndex, A_BOOL is2GHz)
{
    A_UINT16 rateIndex;
    A_UINT8 bytethInArray, bitthInByte;
    A_UINT16 bitthInArray;

    rateIndex = VhtRateIndexToRateGroupTable[userTargetPowerRateIndex];
    GetTgtPowerExtBitInfo (pierth, vhtIdx, rateIndex, is2GHz, &bitthInArray, &bitthInByte, &bytethInArray);
    return (bytethInArray);
}

A_UINT8 GetTgtPowerExtBit (A_UINT16 pierth, A_UINT8 vhtIdx, A_UINT16 rateIndex, A_BOOL is2GHz)
{
    A_UINT8 extByte;
    A_UINT8 bytethInArray, bitthInByte;
    A_UINT16 bitthInArray;

    GetTgtPowerExtBitInfo (pierth, vhtIdx, rateIndex, is2GHz, &bitthInArray, &bitthInByte, &bytethInArray);
    extByte = (is2GHz) ? EEPROM_BASE_POINTER->extTPow2xDelta2G[bytethInArray] : EEPROM_BASE_POINTER->extTPow2xDelta5G[bytethInArray];
    return ((extByte >> bitthInByte) & 1);
}

QC98XX_EEPROM_RATETBL *Qc98xxEepromStructGet(void)
{
    return (QC98XX_EEPROM_RATETBL *)pQc9kEepromArea;
}

A_BOOL SetTgtPowerExtBit (A_INT8 tPow2xDelta, A_UINT16 pierth, A_UINT8 vhtIdx, A_UINT16 rateIndex, A_BOOL is2GHz)
{
    A_UINT8 bytethInArray, bitthInByte;
    A_UINT16 bitthInArray;
    A_UINT8 *extTPow2xDelta;
    A_UINT8 fifthBit;

    extTPow2xDelta = is2GHz ? Qc98xxEepromStructGet()->extTPow2xDelta2G : Qc98xxEepromStructGet()->extTPow2xDelta5G;
    GetTgtPowerExtBitInfo (pierth, vhtIdx, rateIndex, is2GHz, &bitthInArray, &bitthInByte, &bytethInArray);

    fifthBit = (tPow2xDelta >> 4) & 1;
    extTPow2xDelta[bytethInArray] &= ~(1 << bitthInByte);
    extTPow2xDelta[bytethInArray] |= (fifthBit << bitthInByte);
    return TRUE;
}

A_BOOL Qc98xxIsRateInStream (A_UINT32 stream, A_UINT16 rateGroupIndex)
{
        if ((stream == 0 && (rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS5 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS6 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS7 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS8 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS9)) ||
                (stream == 1 && (rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS15 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS16 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS17 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS18 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS19)) ||

                (stream == 2 && (rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS25 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS26 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS27 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS28 ||
                                                rateGroupIndex == WHAL_VHT_TARGET_POWER_RATES_MCS29)))
        {
                return 1;
        }
        return 0;
}

A_UINT32 Qc98xxUserRateIndex2Stream (A_UINT32 userRateGroupIndex)
{
        A_UINT32 stream;

        stream = IS_1STREAM_TARGET_POWER_VHT_RATES(userRateGroupIndex) ? 0 :
                        (IS_2STREAM_TARGET_POWER_VHT_RATES(userRateGroupIndex) ? 1 :
                        (IS_3STREAM_TARGET_POWER_VHT_RATES(userRateGroupIndex) ? 2 : -1));

        return stream;
}

A_UINT8 GetHttPow2xFromStreamAndRateIndex(A_UINT8 *tPow2xBase, A_INT8 *tPow2xDelta, A_UINT8 stream, A_UINT16 rateIndex, A_BOOL byteDelta,
                                          A_UINT16 pierth, A_UINT8 vhtIdx, A_BOOL is2GHz)
{
    A_UINT8 tPow2x, shiftBit;
    A_UINT8 *extTPow2xDelta;
    A_UINT8 maxPier, fifthBit, delta2x;
    A_UINT16 bitthInArray;

        if (byteDelta)
        {
                tPow2x = tPow2xBase[stream] + tPow2xDelta[rateIndex];
        }
        else
        {
        if (is2GHz)
        {
            extTPow2xDelta = Qc98xxEepromStructGet()->extTPow2xDelta2G;
            maxPier = (vhtIdx == CAL_TARGET_POWER_VHT20_INDEX) ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11G_40_TARGET_POWER_CHANS;
        }
        else //5GHz
        {
            extTPow2xDelta = Qc98xxEepromStructGet()->extTPow2xDelta5G;
            maxPier = vhtIdx == CAL_TARGET_POWER_VHT20_INDEX ? WHAL_NUM_11A_20_TARGET_POWER_CHANS :
                        (vhtIdx == CAL_TARGET_POWER_VHT40_INDEX ? WHAL_NUM_11A_40_TARGET_POWER_CHANS : WHAL_NUM_11A_80_TARGET_POWER_CHANS);
        }
                shiftBit = (rateIndex % 2) ? 4 : 0;
        bitthInArray = ((maxPier * vhtIdx) + pierth) * WHAL_NUM_VHT_TARGET_POWER_RATES + rateIndex;
        fifthBit = (extTPow2xDelta[bitthInArray >> 3] >> (bitthInArray % 8)) & 1;
        delta2x = ((tPow2xDelta[rateIndex >> 1] >> shiftBit) & 0x0f) | (fifthBit << 4);
                tPow2x = tPow2xBase[stream] + delta2x;
        }
        return tPow2x;
}

A_INT32 Qc98xxRateGroupIndex2Stream (A_UINT16 rateGroupIndex, A_UINT16 neighborRateIndex)
{
        A_INT32 stream;
        switch (rateGroupIndex)
        {
                case WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20:    //not sure what stream these 3 belong to
                case WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22:
                case WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24:
                        if (neighborRateIndex <= WHAL_VHT_TARGET_POWER_RATES_MCS9)
                        {
                                stream = 0;
                        }
                        else if (neighborRateIndex <= WHAL_VHT_TARGET_POWER_RATES_MCS19)
                        {
                                stream = 1;
                        }
                        else
                        {
                                stream = 2;
                        }
                case WHAL_VHT_TARGET_POWER_RATES_MCS5:
                case WHAL_VHT_TARGET_POWER_RATES_MCS6:
                case WHAL_VHT_TARGET_POWER_RATES_MCS7:
                case WHAL_VHT_TARGET_POWER_RATES_MCS8:
                case WHAL_VHT_TARGET_POWER_RATES_MCS9:
                        stream = 0;
                        break;

                case WHAL_VHT_TARGET_POWER_RATES_MCS15:
                case WHAL_VHT_TARGET_POWER_RATES_MCS16:
                case WHAL_VHT_TARGET_POWER_RATES_MCS17:
                case WHAL_VHT_TARGET_POWER_RATES_MCS18:
                case WHAL_VHT_TARGET_POWER_RATES_MCS19:
                        stream = 1;
                        break;

                case WHAL_VHT_TARGET_POWER_RATES_MCS25:
                case WHAL_VHT_TARGET_POWER_RATES_MCS26:
                case WHAL_VHT_TARGET_POWER_RATES_MCS27:
                case WHAL_VHT_TARGET_POWER_RATES_MCS28:
                case WHAL_VHT_TARGET_POWER_RATES_MCS29:
                        stream = 2;
                        break;

                default:
                        stream = -1;
                        break;
        }
        return stream;
}

//
// This is used when delta(0) > 0xf
// Adjust the smallest delta to 0, and its base accordingly.
// Then adjust the rest
//
void AdjustTargetPowerLocalArrays(A_UINT8 *tPow2xBaseLocal, A_INT8 *tPow2xDeltaLocal, A_INT32 numOfStreams)
{
        A_INT32 i, stream;
        A_INT32 txPow2x, txPow2xLow, streamLow, rateLow;
        A_INT8 baseDelta;

        streamLow = 0;
        rateLow = WHAL_NUM_VHT_TARGET_POWER_RATES-1;
        txPow2xLow = tPow2xBaseLocal[streamLow] + tPow2xDeltaLocal[rateLow];

        // Look for the smallest target power
        for (i = 0; i < WHAL_NUM_VHT_TARGET_POWER_RATES; ++i)
        {
                stream = Qc98xxRateGroupIndex2Stream (i, i+3);
                txPow2x = tPow2xBaseLocal[stream] + tPow2xDeltaLocal[i];
                if (txPow2x < txPow2xLow)
                {
                        txPow2xLow = txPow2x;
                        streamLow = stream;
                        rateLow = i;
                }
        }
        baseDelta = tPow2xBaseLocal[streamLow] - txPow2xLow;

        // adjust the bases
        for (i = 0; i < numOfStreams; ++i)
        {
                tPow2xBaseLocal[i] -= baseDelta;
        }
        // adjust deltas
        tPow2xDeltaLocal[rateLow] = 0;
        for (i = 0; i < WHAL_NUM_VHT_TARGET_POWER_RATES; ++i)
        {
                if (i != rateLow)
                {
                        tPow2xDeltaLocal[i] += baseDelta;
                }
        }
}

A_BOOL SetHttPow2x (A_UINT8 value, A_UINT8 *tPow2xBase, A_UINT8 *tPow2xDelta, A_UINT16 userTargetPowerRateIndex,
                  A_INT32 *baseChanged, A_UINT8 flag, A_INT32 num, A_UINT16 pierth, A_UINT8 vhtIdx, A_BOOL is2GHz)
{
    A_UINT8 tPow2x, shiftBit;
    A_UINT8 tPow2xLowest;
    A_UINT16 targetPowerRateIndex;
    A_INT8 deltaVal, baseDelta;//, mcs0Delta;
    A_INT32 i, stream;
    // Stored the results in these 2 arrays tPow2xBaseLocal and tPow2xDeltaLocal.
    // Only copy back to EEPROM area when everything is good at the end
    static A_UINT8 tPow2xBaseLocal[WHAL_NUM_STREAMS] = {0};
    // Store delta as signed to check for -ve later. THe deltas should not be -ve nor > 0xf
    static A_INT8  tPow2xDeltaLocal[WHAL_NUM_VHT_TARGET_POWER_RATES] = {0};
    A_INT32 adjustAttempt;

    if ((stream = Qc98xxUserRateIndex2Stream(userTargetPowerRateIndex)) < 0)
    {
        return FALSE;
    }
    targetPowerRateIndex = VhtRateIndexToRateGroupTable[userTargetPowerRateIndex];
    shiftBit = (targetPowerRateIndex % 2) ? 4 : 0;

    // copy tPow2xBase and tPow2xDelta to local memory if this is the first value in the set command
    if (flag & FLAG_FIRST)
    {
        OS_MEMCPY(tPow2xBaseLocal, tPow2xBase, sizeof(tPow2xBaseLocal));
        for (i = 0; i < sizeof(tPow2xDeltaLocal); i=i+2)
        {
            tPow2xDeltaLocal[i] = (tPow2xDelta[i>>1] & 0xf) |
                                    GetTgtPowerExtBit(pierth, vhtIdx, i, is2GHz) << 4;
            tPow2xDeltaLocal[i+1] = ((tPow2xDelta[i>>1] >> 4) & 0xf) |
                                    GetTgtPowerExtBit(pierth, vhtIdx, i+1, is2GHz) << 4;
        }
    }
    tPow2x = (A_UINT8)(value);

    tPow2xLowest = tPow2xBase[0];
    // Look for the lowest target power
    for (i = 1; i < WHAL_NUM_STREAMS; ++i)
    {
        if (tPow2xBaseLocal[i] < tPow2xLowest)
        {
            tPow2xLowest = tPow2xBaseLocal[i];
        }
    }
    // if no change in value, do nothing
    if (tPow2x == GetHttPow2xFromStreamAndRateIndex(tPow2xBaseLocal, tPow2xDeltaLocal, stream, targetPowerRateIndex, TRUE, pierth, vhtIdx, is2GHz))
    {
    }
    // if the input power is greater or equal to the bases and the rate is not in the 1st 3 rate groups
    //else if ((tPow2x >= tPow2xLowest) && NOT_VHT_TARGET_RATE_LOW_GROUPS(userTargetPowerRateIndex))
    else if ((tPow2x >= tPow2xLowest) &&
                (NOT_VHT_TARGET_RATE_LOW_GROUPS(userTargetPowerRateIndex) ||
                    (flag == 0 && !VHT_TARGET_RATE_0_10_20(userTargetPowerRateIndex))))
    {
        // Just update the delta for that rate
        deltaVal = tPow2x - tPow2xBaseLocal[stream];
        //if (deltaVal > 0xf)
        //{
        //  UserPrA_INT32("Error - Reduce the target power value. Max acceptable value is %lf\n", ((tPow2xBase[stream] + 0xf) /2));
        //  UserPrA_INT32("Or set the target power for the rate having the lowest target power first.\n");
        //  return FALSE;
        //}
        tPow2xDeltaLocal[targetPowerRateIndex] = deltaVal;
    }
    // MCS0 ... MCS4
    else if (VHT_TARGET_RATE_0_10_20(userTargetPowerRateIndex))
    {
        baseDelta = tPow2xBaseLocal[stream] + tPow2xDeltaLocal[targetPowerRateIndex] - tPow2x;
        if (flag & FLAG_FIRST)
        {
            tPow2xDeltaLocal[targetPowerRateIndex] -= baseDelta;
            for (i = 0; i < WHAL_NUM_STREAMS; ++i)
            {
                if (i != stream)
                {
                    tPow2xBaseLocal[i] += baseDelta;
                }
            }
            for (i = 0; i < WHAL_NUM_VHT_TARGET_POWER_RATES; ++i)
            {
                if (!Qc98xxIsRateInStream(stream, i))
                {
                    tPow2xDeltaLocal[i] -= baseDelta;
                }
            }
        }
        else
        {
            // adjust the stream base only
            tPow2xBaseLocal[stream] -= baseDelta;
            for (i = (stream == 1 ? 8 : 13); i < (stream == 1 ? 13 : 18); ++i)
            {
                //if (!Qc98xxIsRateInStream(stream, i))
                //{
                    tPow2xDeltaLocal[i] += baseDelta;
                //}
            }
        }
        *baseChanged = 1;
    }
    // For the rest, we have to adjust bases and deltas
    // use tPow2xBaseLocal and tPow2xDeltaLocal, in case there is error
    else
    {
        baseDelta = tPow2xBaseLocal[stream] - tPow2x;

        // adjust the bases
        for (i = 0; i < WHAL_NUM_STREAMS; ++i)
        {
            tPow2xBaseLocal[i] -= baseDelta;
        }
        // adjust deltas
        tPow2xDeltaLocal[targetPowerRateIndex] = 0;
        for (i = 0; i < WHAL_NUM_VHT_TARGET_POWER_RATES; ++i)
        {
            if (i != targetPowerRateIndex)
            {
                tPow2xDeltaLocal[i] += baseDelta;
            }
        }
        *baseChanged = 1;
    }
    if (flag & FLAG_LAST)
    {
        // Check for errors before copy to EEPROM area
        adjustAttempt = 3;
        while (adjustAttempt)
        {
            for (i = 0; i < WHAL_NUM_VHT_TARGET_POWER_RATES; i++)
            {
                if (tPow2xDeltaLocal[i] < 0)
                {
                    return FALSE;
                }
                else if(tPow2xDeltaLocal[i] > 0xf)
                {
                    // adjust the bases and deltas if possible
                    AdjustTargetPowerLocalArrays(tPow2xBaseLocal, tPow2xDeltaLocal, WHAL_NUM_STREAMS);
                    break;
                }
            }
            if (i == WHAL_NUM_VHT_TARGET_POWER_RATES)
            {
                break;
            }
            adjustAttempt--;
        }

        // Successfully set the target powers
        if (*baseChanged || num > 1)
        {
            for (i = 0; i < WHAL_NUM_STREAMS; ++i)
            {
                tPow2xBase[i] = tPow2xBaseLocal[i];
            }
            for (i = 0; i < WHAL_NUM_VHT_TARGET_POWER_RATES; i=i+2)
            {
                tPow2xDelta[i>>1] = (tPow2xDeltaLocal[i] & 0xf) | ((tPow2xDeltaLocal[i+1] << 4) & 0xf0);
                SetTgtPowerExtBit (tPow2xDeltaLocal[i], pierth, vhtIdx, i, is2GHz);
                SetTgtPowerExtBit (tPow2xDeltaLocal[i+1], pierth, vhtIdx, i+1, is2GHz);
            }
        }
        else
        {
            if (shiftBit == 0)
            {
                tPow2xDelta[targetPowerRateIndex>>1] = (tPow2xDeltaLocal[targetPowerRateIndex] & 0xf) | ((tPow2xDeltaLocal[targetPowerRateIndex+1] << 4) & 0xf0);
            }
            else
            {
                tPow2xDelta[targetPowerRateIndex>>1] = (tPow2xDeltaLocal[targetPowerRateIndex-1] & 0xf) | ((tPow2xDeltaLocal[targetPowerRateIndex] << 4) & 0xf0);
            }
            SetTgtPowerExtBit (tPow2xDeltaLocal[targetPowerRateIndex], pierth, vhtIdx, targetPowerRateIndex, is2GHz);
        }
    }
    return TRUE;
}


A_BOOL IsDiffBetweenTargetPowersTooBig (A_UINT8 *value, A_INT32 size)
{
    A_INT32 i;
    A_UINT8 max, min;

    max = min = value[0];
    for (i = 0; i < size; ++i)
    {
        if (value[i] > max)
        {
            max = value[i];
        }
        else if (value[i] < min)
        {
            min = value[i];
        }
    }
    return ((max - min) > 0x1f);
}


// userTargetPowerRateIndex: see TARGET_POWER_HT_RATES in Qc98xxEepromStruct.h
A_UINT8 GetHttPow2x(A_UINT8 *tPow2xBase, A_INT8 *tPow2xDelta, A_UINT16 userTargetPowerRateIndex, A_BOOL byteDelta,
                    A_UINT16 pierth, A_UINT8 vhtIdx, A_BOOL is2GHz)
{
        A_UINT8 tPow2x;
        A_INT32 stream;
        A_UINT16 targetPowerRateIndex;

        stream = Qc98xxUserRateIndex2Stream(userTargetPowerRateIndex);
        targetPowerRateIndex = VhtRateIndexToRateGroupTable[userTargetPowerRateIndex];
        if (stream < 0)
        {
                return 0xff;
        }

        tPow2x = GetHttPow2xFromStreamAndRateIndex(tPow2xBase, tPow2xDelta, stream, targetPowerRateIndex, byteDelta,
                                                    pierth, vhtIdx, is2GHz);
        return tPow2x;
}

A_BOOL IsValidInputTargetPowers(A_UINT8 *value, A_UINT8 *tPow2xBase, A_UINT8 *tPow2xDelta, A_INT32 j0, A_INT32 iv, A_INT32 num,
                                A_UINT16 pierth, A_UINT8 vhtIdx, A_BOOL is2GHz)
{
        A_INT32 j, j1, i;
        A_UINT8 endValue[VHT_TARGET_RATE_LAST];

        i = iv;
        for (j=0; j < j0; j++)
        {
                endValue[j] = GetHttPow2x(tPow2xBase, (A_INT8*)tPow2xDelta, j, FALSE, pierth, vhtIdx, is2GHz);
        }

        j1 = (j0 + num < VHT_TARGET_RATE_LAST) ? (j0 + num) : VHT_TARGET_RATE_LAST;

        for (j=j0; j < j1; j++)
    {
                endValue[j] = (A_UINT8)(value[i++]);
        }
        for (; j < VHT_TARGET_RATE_LAST; j++)
        {
                endValue[j] = GetHttPow2x(tPow2xBase, (A_INT8*)tPow2xDelta, j, FALSE, pierth, vhtIdx, is2GHz);
        }

        if (IsDiffBetweenTargetPowersTooBig(endValue, VHT_TARGET_RATE_LAST))
        {
                return FALSE;
        }
        return TRUE;
}

A_INT32 Qc98xxCalTGTPwrCCKSet(A_UINT8 *value, A_INT32 ix, A_INT32 iy, A_INT32 iz, A_INT32 num, A_INT32 iBand)
{
    A_INT32 i, j, j0, iv=0;
    for (i=ix; i<WHAL_NUM_11B_TARGET_POWER_CHANS; i++)
    {
        if (iv>=num)
            break;
        if (i==ix)
            j0=iy;
        else
            j0=0;
        for (j=j0; j<WHAL_NUM_LEGACY_TARGET_POWER_RATES; j++)
        {
            if (iBand==band_BG) {
                EEPROM_BASE_POINTER->targetPowerCck[i].tPow2x[j] = (A_UINT8)(value[iv++]);  // eep value is A_UINT8 of entered value
            } else
                return ERR_VALUE_BAD;
            if (iv>=num)
                break;
        }
    }
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrLegacyOFDMSet(A_UINT8 *value, A_INT32 ix, A_INT32 iy, A_INT32 iz, A_INT32 num, A_INT32 iBand)
{
    A_INT32 i, j, j0, iv=0;
    A_INT32 iMaxPier;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11A_20_TARGET_POWER_CHANS;
    for (i=ix; i<iMaxPier; i++)
    {
        if (iv>=num)
            break;
        if (i==ix)
            j0=iy;
        else
            j0=0;
        for (j=j0; j<WHAL_NUM_LEGACY_TARGET_POWER_RATES; j++)
        {
            if (iBand==band_BG) {
                EEPROM_BASE_POINTER->targetPower2G[i].tPow2x[j] = (A_UINT8)(value[iv++]);   // eep value is A_UINT8 of entered value
            } else {
                EEPROM_BASE_POINTER->targetPower5G[i].tPow2x[j] = (A_UINT8)(value[iv++]);
            }
            if (iv>=num)
                break;
        }
    }
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrHT20Set(A_UINT8 *value, A_INT32 ix, A_INT32 iy, A_INT32 iz, A_INT32 num, A_INT32 iBand)
{
    A_INT32 i, j, j0, iv=0;
    A_INT32 iMaxPier, jMaxRate;
    A_INT32 baseChanged;
    A_UINT8 *tPow2xBase, *tPow2xDelta;
    A_UINT8 flag;
    A_BOOL Ok;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11A_20_TARGET_POWER_CHANS;
    jMaxRate = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_RATES : WHAL_NUM_11A_20_TARGET_POWER_RATES;

    if (ix >= iMaxPier || iy >= VHT_TARGET_RATE_LAST)
    {
        return ERR_RETURN;
    }

    for (i=ix; i<iMaxPier; i++)
    {
        if (iv>=num)
            break;
        if (i==ix)
            j0=iy;
        else
            j0=0;

        // Since the deltas stored in nibbles, the difference of the target powers in a pier should not be greater than 15x.5 steps
        // construct endValue to check
        if (iBand==band_BG)
        {
            tPow2xBase = EEPROM_BASE_POINTER->targetPower2GVHT20[i].tPow2xBase;
            tPow2xDelta = EEPROM_BASE_POINTER->targetPower2GVHT20[i].tPow2xDelta;
        }
        else
        {
            tPow2xBase = EEPROM_BASE_POINTER->targetPower5GVHT20[i].tPow2xBase;
            tPow2xDelta = EEPROM_BASE_POINTER->targetPower5GVHT20[i].tPow2xDelta;
        }
        if (!IsValidInputTargetPowers(value, tPow2xBase, tPow2xDelta, j0, iv, num, i, CAL_TARGET_POWER_VHT20_INDEX, iBand==band_BG))
        {
            return ERR_RETURN;
        }

        flag = FLAG_FIRST;
        baseChanged = 0;
        // The set command can set just one or the whole rates per channel
        for (j=j0; j<VHT_TARGET_RATE_LAST; j++)
        {
            if ((iv + 1) == num)
            {
                flag |= FLAG_LAST;
            }

            Ok = SetHttPow2x (value[iv++], tPow2xBase, tPow2xDelta, j, &baseChanged, flag, num, i, CAL_TARGET_POWER_VHT20_INDEX, iBand==band_BG);

            if (!Ok)
            {
                return ERR_RETURN;
            }
            //baseChangedCount += baseChanged;
            if (iv>=num)
            {
                break;
            }
            flag = 0;
        }
        if (iBand==band_BG)
        {
            if (baseChanged)
            {
                //EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower2GVHT20[i].tPow2xBase, WHAL_NUM_STREAMS);
                //EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower2GVHT20[i].tPow2xDelta, (jMaxRate >>1));
                //EEPROM_CONFIG_DIFF_CHANGE_ARRAY(extTPow2xDelta2G, 1);
            }
            else
            {
                //EEPROM_CONFIG_DIFF_CHANGE(targetPower2GVHT20[i].tPow2xDelta[(j)>>1]);
                //EEPROM_CONFIG_DIFF_CHANGE(extTPow2xDelta2G[GetTgtPowerExtByteIndex(i, CAL_TARGET_POWER_VHT20_INDEX, j, TRUE)]);
            }
        }
        else
        {
            if (baseChanged)
            {
                //EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower5GVHT20[i].tPow2xBase, WHAL_NUM_STREAMS);
            //  EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower5GVHT20[i].tPow2xDelta, (jMaxRate >>1));
               // EEPROM_CONFIG_DIFF_CHANGE_ARRAY(extTPow2xDelta5G, 1);
            }
            else
            {
        //      EEPROM_CONFIG_DIFF_CHANGE(targetPower5GVHT20[i].tPow2xDelta[(j)>>1]);
                //EEPROM_CONFIG_DIFF_CHANGE(extTPow2xDelta5G[GetTgtPowerExtByteIndex(i, CAL_TARGET_POWER_VHT20_INDEX, j,FALSE)]);
            }
        }
    }
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrHT40Set(A_UINT8 *value, A_INT32 ix, A_INT32 iy, A_INT32 iz, A_INT32 num, A_INT32 iBand)
{
    A_INT32 i, j, j0, iv=0;
    A_INT32 iMaxPier, jMaxRate;
    A_INT32 baseChanged;
    A_UINT8 *tPow2xBase, *tPow2xDelta;
    A_UINT8 flag;
    A_BOOL Ok;


    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_40_TARGET_POWER_CHANS : WHAL_NUM_11A_40_TARGET_POWER_CHANS;
    jMaxRate = (iBand==band_BG) ? WHAL_NUM_11G_40_TARGET_POWER_RATES : WHAL_NUM_11A_40_TARGET_POWER_RATES;

    if (ix >= iMaxPier || iy >= VHT_TARGET_RATE_LAST)
    {
        return ERR_RETURN;
    }

    for (i=ix; i<iMaxPier; i++)
    {
        if (iv>=num)
            break;
        if (i==ix)
            j0=iy;
        else
            j0=0;

        // Since the deltas stored in nibbles, the difference of the target powers in a pier should not be greater than 15x.5 steps
        // construct endValue to check
        if (iBand==band_BG)
        {
            tPow2xBase = EEPROM_BASE_POINTER->targetPower2GVHT40[i].tPow2xBase;
            tPow2xDelta = EEPROM_BASE_POINTER->targetPower2GVHT40[i].tPow2xDelta;
        }
        else
        {
            tPow2xBase = EEPROM_BASE_POINTER->targetPower5GVHT40[i].tPow2xBase;
            tPow2xDelta = EEPROM_BASE_POINTER->targetPower5GVHT40[i].tPow2xDelta;
        }
        if (!IsValidInputTargetPowers(value, tPow2xBase, tPow2xDelta, j0, iv, num, i, CAL_TARGET_POWER_VHT40_INDEX, iBand==band_BG))
        {
            return ERR_RETURN;
        }

        flag = FLAG_FIRST;
        baseChanged = 0;
        // The set command can set just one or the whole rates per channel
        for (j=j0; j<VHT_TARGET_RATE_LAST; j++)
        {
            if ((iv + 1) == num)
            {
                flag |= FLAG_LAST;
            }

            Ok = SetHttPow2x (value[iv++], tPow2xBase, tPow2xDelta, j, &baseChanged, flag, num, i, CAL_TARGET_POWER_VHT40_INDEX, iBand==band_BG);

            if (!Ok)
            {
                return ERR_RETURN;
            }
            //baseChangedCount += baseChanged;
            if (iv>=num)
            {
                break;
            }
            flag = 0;
        }
        if (iBand==band_BG)
        {
            if (baseChanged)
            {
        //      EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower2GVHT40[i].tPow2xBase, WHAL_NUM_STREAMS);
        //      EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower2GVHT40[i].tPow2xDelta, (jMaxRate >>1));
               // EEPROM_CONFIG_DIFF_CHANGE_ARRAY(extTPow2xDelta2G, 1);
            }
            else
            {
        //      EEPROM_CONFIG_DIFF_CHANGE(targetPower2GVHT40[i].tPow2xDelta[(j)>>1]);
                //EEPROM_CONFIG_DIFF_CHANGE(extTPow2xDelta2G[GetTgtPowerExtByteIndex(i, CAL_TARGET_POWER_VHT40_INDEX, j,TRUE)]);
            }
        }
        else
        {
            if (baseChanged)
            {
        //      EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower5GVHT40[i].tPow2xBase, WHAL_NUM_STREAMS);
        //      EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower5GVHT40[i].tPow2xDelta, (jMaxRate >>1));
                //EEPROM_CONFIG_DIFF_CHANGE_ARRAY(extTPow2xDelta5G, 1);
            }
            else
            {
            //  EEPROM_CONFIG_DIFF_CHANGE(targetPower5GVHT40[i].tPow2xDelta[(j)>>1]);
                //EEPROM_CONFIG_DIFF_CHANGE(extTPow2xDelta5G[GetTgtPowerExtByteIndex(i, CAL_TARGET_POWER_VHT40_INDEX, j,FALSE)]);
            }
        }
    }
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrHT80Set(A_UINT8 *value, A_INT32 ix, A_INT32 iy, A_INT32 iz, A_INT32 num, A_INT32 iBand)
{
    A_INT32 i, j, j0, iv=0;
    A_INT32 iMaxPier, jMaxRate;
    A_INT32 baseChanged;
    A_UINT8 flag;
    A_BOOL Ok;

    if (iBand==band_BG)
    {
        return ERR_VALUE_BAD;
    }
    iMaxPier = WHAL_NUM_11A_80_TARGET_POWER_CHANS;
    jMaxRate = WHAL_NUM_11A_80_TARGET_POWER_RATES;

    if (ix >= iMaxPier || iy >= VHT_TARGET_RATE_LAST)
    {
        return ERR_RETURN;
    }

    for (i=ix; i<iMaxPier; i++)
    {
        if (iv>=num)
            break;
        if (i==ix)
            j0=iy;
        else
            j0=0;

        // Since the deltas stored in nibbles, the difference of the target powers in a pier should not be greater than 15x.5 steps
        // construct endValue to check
        if (!IsValidInputTargetPowers(value, EEPROM_BASE_POINTER->targetPower5GVHT80[i].tPow2xBase,
                                        EEPROM_BASE_POINTER->targetPower5GVHT80[i].tPow2xDelta, j0, iv, num,
                                        i, CAL_TARGET_POWER_VHT80_INDEX, FALSE))
        {
            return ERR_RETURN;
        }

        flag = FLAG_FIRST;
        baseChanged = 0;
        // The set command can set just one or the whole rates per channel
        for (j=j0; j<VHT_TARGET_RATE_LAST; j++)
        {
            if ((iv + 1) == num)
            {
                flag |= FLAG_LAST;
            }

            Ok = SetHttPow2x (value[iv++], EEPROM_BASE_POINTER->targetPower5GVHT80[i].tPow2xBase,
                                EEPROM_BASE_POINTER->targetPower5GVHT80[i].tPow2xDelta, j, &baseChanged, flag, num,
                                i, CAL_TARGET_POWER_VHT80_INDEX, FALSE);

            if (!Ok)
            {
                return ERR_RETURN;
            }
            //baseChangedCount += baseChanged;
            if (iv>=num)
            {
                break;
            }
            flag = 0;
        }
        if (baseChanged)
        {
//          EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower5GVHT80[i].tPow2xBase, WHAL_NUM_STREAMS);
//          EEPROM_CONFIG_DIFF_CHANGE_ARRAY(targetPower5GVHT80[i].tPow2xDelta, (jMaxRate >>1));
 //           EEPROM_CONFIG_DIFF_CHANGE_ARRAY(extTPow2xDelta5G, 1);
        }
        else
        {
//          EEPROM_CONFIG_DIFF_CHANGE(targetPower5GVHT80[i].tPow2xDelta[(j)>>1]);
//            EEPROM_CONFIG_DIFF_CHANGE(extTPow2xDelta5G[GetTgtPowerExtByteIndex(i, CAL_TARGET_POWER_VHT80_INDEX, j, FALSE)]);
        }
    }
    return VALUE_OK;
}


A_INT32 Qc98xxCalTGTPwrCCKGet(A_UINT8 *value, A_INT32 ix, A_INT32 iy, A_INT32 iz, A_INT32 *num, A_INT32 iBand)
{
    A_INT32 i, j, iv=0, iMaxPier, jMaxRate=WHAL_NUM_LEGACY_TARGET_POWER_RATES;
    iMaxPier=WHAL_NUM_11B_TARGET_POWER_CHANS;
    if (iy<0 || iy>=jMaxRate)
    {
        if (ix<0 || ix>=iMaxPier)
        {
            // get all i, all j
            for (i=0; i<iMaxPier; i++)
            {
                for (j=0; j<jMaxRate; j++)
                {
                    value[iv++] = Qc98xxEepromStructGet()->targetPowerCck[i].tPow2x[j];
                }
            }
            *num = jMaxRate*iMaxPier;
        }
        else
        { // get all j for ix chain
            for (j=0; j<jMaxRate; j++)
            {
                value[iv++] = Qc98xxEepromStructGet()->targetPowerCck[ix].tPow2x[j];
            }
            *num = jMaxRate;
        }
    }
    else
    {
        if (ix<0 || ix>=iMaxPier)
        {
            for (i=0; i<iMaxPier; i++)
            {
                value[iv++] = Qc98xxEepromStructGet()->targetPowerCck[i].tPow2x[iy];
                *num = iMaxPier;
            }
        }
        else
        {
            value[0] = Qc98xxEepromStructGet()->targetPowerCck[ix].tPow2x[iy];
            *num = 1;
        }
    }
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrLegacyOFDMGet(A_UINT8 *value, A_INT32 ix, A_INT32 iy, A_INT32 iz, A_INT32 *num, A_INT32 iBand)
{
    A_INT32 i, j, iv=0, iMaxPier, jMaxRate=WHAL_NUM_LEGACY_TARGET_POWER_RATES;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11A_20_TARGET_POWER_CHANS;

    if (iy<0 || iy>=jMaxRate)
    {
        if (ix<0 || ix>=iMaxPier)
        {
            // get all i, all j
            for (i=0; i<iMaxPier; i++)
            {
                for (j=0; j<jMaxRate; j++)
                {
                    if (iBand==band_BG) {
                        value[iv++] = (Qc98xxEepromStructGet()->targetPower2G[i].tPow2x[j]);
                    } else {
                        value[iv++] = (Qc98xxEepromStructGet()->targetPower5G[i].tPow2x[j]);
                    }
                }
            }
            *num = jMaxRate*iMaxPier;
        }
        else
        { // get all j for ix chain
            for (j=0; j<jMaxRate; j++)
            {
                if (iBand==band_BG) {
                    value[iv++] = (Qc98xxEepromStructGet()->targetPower2G[ix].tPow2x[j]);
                } else {
                    value[iv++] = (Qc98xxEepromStructGet()->targetPower5G[ix].tPow2x[j]);
                }
            }
            *num = jMaxRate;
        }
    }
    else
    {
        if (ix<0 || ix>=iMaxPier)
        {
            for (i=0; i<iMaxPier; i++)
            {
                if (iBand==band_BG) {
                    value[iv++] = (Qc98xxEepromStructGet()->targetPower2G[i].tPow2x[iy]);
                } else {
                    value[iv++] = (Qc98xxEepromStructGet()->targetPower5G[i].tPow2x[iy]);
                }
                *num = iMaxPier;
            }
        }
        else
        {
            if (iBand==band_BG) {
                value[0] =(Qc98xxEepromStructGet()->targetPower2G[ix].tPow2x[iy]);
            } else {
                value[0] = (Qc98xxEepromStructGet()->targetPower5G[ix].tPow2x[iy]);
            }
            *num = 1;
        }
    }
    return VALUE_OK;
}


A_INT32 Qc98xxCalTGTPwrHT20Get(A_UINT8 *value, A_INT32 ix, A_INT32 iy, A_INT32 iz, A_INT32 *num, A_INT32 iBand)
{
    A_INT32 i, j, iv=0, iMaxPier, jMaxRate;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11A_20_TARGET_POWER_CHANS;
    jMaxRate = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_RATES : WHAL_NUM_11A_20_TARGET_POWER_RATES;

    if (ix >= iMaxPier || iy >= VHT_TARGET_RATE_LAST)
    {
        return ERR_RETURN;
    }
    if (iy >= jMaxRate)
    {
        return ERR_RETURN;
    }

    jMaxRate = VHT_TARGET_RATE_LAST;

    if (iy<0 || iy>=jMaxRate)
    {
        if (ix<0 || ix>=iMaxPier)
        {
            // get all i, all j
            for (i=0; i<iMaxPier; i++)
            {
                for (j=0; j<jMaxRate; j++)
                {
                    if (iBand==band_BG) {
                        value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xBase,
                                                            (A_INT8*)Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xDelta, j, FALSE,
                                                            i, CAL_TARGET_POWER_VHT20_INDEX, TRUE));
                    } else {
                        value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xBase,
                                                            (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xDelta, j, FALSE,
                                                            i, CAL_TARGET_POWER_VHT20_INDEX, FALSE));
                    }
                }
            }
            *num = jMaxRate*iMaxPier;
        }
        else
        { // get all j for ix chain
            for (j=0; j<jMaxRate; j++)
            {
                if (iBand==band_BG) {
                    value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT20[ix].tPow2xBase,
                                                        (A_INT8*)Qc98xxEepromStructGet()->targetPower2GVHT20[ix].tPow2xDelta, j, FALSE,
                                                        ix, CAL_TARGET_POWER_VHT20_INDEX, TRUE));
                } else {
                    value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT20[ix].tPow2xBase,
                                                        (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT20[ix].tPow2xDelta, j, FALSE,
                                                        ix, CAL_TARGET_POWER_VHT20_INDEX, FALSE));
                }
            }
            *num = jMaxRate;
        }
    }
    else
    {
        if (ix<0 || ix>=iMaxPier)
        {
            for (i=0; i<iMaxPier; i++)
            {
                if (iBand==band_BG) {
                    value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xBase,
                                                        (A_INT8*)Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xDelta, iy, FALSE,
                                                        i, CAL_TARGET_POWER_VHT20_INDEX, TRUE));
                } else {
                    value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xBase,
                                                        (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xDelta, iy, FALSE,
                                                        i, CAL_TARGET_POWER_VHT20_INDEX, FALSE));
                }
                *num = iMaxPier;
            }
        }
        else
        {
            if (iBand==band_BG) {
                value[0] =(GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT20[ix].tPow2xBase,
                                                (A_INT8*)Qc98xxEepromStructGet()->targetPower2GVHT20[ix].tPow2xDelta, iy, FALSE,
                                                ix, CAL_TARGET_POWER_VHT20_INDEX, TRUE));
            } else {
                value[0] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT20[ix].tPow2xBase,
                                                (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT20[ix].tPow2xDelta, iy, FALSE,
                                                ix, CAL_TARGET_POWER_VHT20_INDEX, FALSE));
            }
            *num = 1;
        }
    }
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrHT40Get(A_UINT8 *value, int ix, int iy, int iz, int *num, int iBand)
{
    int i, j, iv=0, iMaxPier, jMaxRate;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_40_TARGET_POWER_CHANS : WHAL_NUM_11A_40_TARGET_POWER_CHANS;
    jMaxRate = (iBand==band_BG) ? WHAL_NUM_11G_40_TARGET_POWER_RATES : WHAL_NUM_11A_40_TARGET_POWER_RATES;

    if (ix >= iMaxPier || iy >= VHT_TARGET_RATE_LAST)
    {
        return ERR_RETURN;
    }
    if (iy >= jMaxRate)
    {
        return ERR_RETURN;
    }

    jMaxRate = VHT_TARGET_RATE_LAST;

    if (iy<0 || iy>=jMaxRate)
    {
        if (ix<0 || ix>=iMaxPier)
        {
            // get all i, all j
            for (i=0; i<iMaxPier; i++)
            {
                for (j=0; j<jMaxRate; j++)
                {
                    if (iBand==band_BG) {
                        value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xBase,
                                                            (A_INT8*)Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xDelta, j, FALSE,
                                                            i, CAL_TARGET_POWER_VHT40_INDEX, TRUE));
                    } else {
                        value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xBase,
                                                            (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xDelta, j, FALSE,
                                                            i, CAL_TARGET_POWER_VHT40_INDEX, FALSE));
                    }
                }
            }
            *num = jMaxRate*iMaxPier;
        }
        else
        { // get all j for ix chain
            for (j=0; j<jMaxRate; j++)
            {
                if (iBand==band_BG) {
                    value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT40[ix].tPow2xBase,
                                                        (A_INT8*)Qc98xxEepromStructGet()->targetPower2GVHT40[ix].tPow2xDelta, j, FALSE,
                                                        ix, CAL_TARGET_POWER_VHT40_INDEX, TRUE));
                } else {
                    value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT40[ix].tPow2xBase,
                                                        (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT40[ix].tPow2xDelta, j, FALSE,
                                                        ix, CAL_TARGET_POWER_VHT40_INDEX, FALSE));
                }
            }
            *num = jMaxRate;
        }
    }
    else
    {
        if (ix<0 || ix>=iMaxPier)
        {
            for (i=0; i<iMaxPier; i++)
            {
                if (iBand==band_BG) {
                    value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xBase,
                                                        (A_INT8*)Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xDelta, iy, FALSE,
                                                        i, CAL_TARGET_POWER_VHT40_INDEX, TRUE));
                } else {
                    value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xBase,
                                                        (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xDelta, iy, FALSE,
                                                        i, CAL_TARGET_POWER_VHT40_INDEX, FALSE));
                }
                *num = iMaxPier;
            }
        }
        else
        {
            if (iBand==band_BG) {
                value[0] =(GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT40[ix].tPow2xBase,
                                                (A_INT8*)Qc98xxEepromStructGet()->targetPower2GVHT40[ix].tPow2xDelta, iy, FALSE,
                                                ix, CAL_TARGET_POWER_VHT40_INDEX, TRUE));
            } else {
                value[0] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT40[ix].tPow2xBase,
                                                (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT40[ix].tPow2xDelta, iy, FALSE,
                                                ix, CAL_TARGET_POWER_VHT40_INDEX, FALSE));
            }
            *num = 1;
        }
    }
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrHT80Get(A_UINT8 *value, int ix, int iy, int iz, int *num, int iBand)
{
    int i, j, iv=0, iMaxPier, jMaxRate;

    if (iBand==band_BG)
    {
        return ERR_VALUE_BAD;
    }
    iMaxPier = WHAL_NUM_11A_80_TARGET_POWER_CHANS;
    jMaxRate = WHAL_NUM_11A_80_TARGET_POWER_RATES;

    if (ix >= iMaxPier || iy >= VHT_TARGET_RATE_LAST)
    {
        return ERR_RETURN;
    }
    if (iy >= jMaxRate)
    {
        return ERR_RETURN;
    }

    jMaxRate = VHT_TARGET_RATE_LAST;

    if (iy<0 || iy>=jMaxRate)
    {
        if (ix<0 || ix>=iMaxPier)
        {
            // get all i, all j
            for (i=0; i<iMaxPier; i++)
            {
                for (j=0; j<jMaxRate; j++)
                {
                    value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xBase,
                                                        (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xDelta, j, FALSE,
                                                        i, CAL_TARGET_POWER_VHT80_INDEX, FALSE));
                }
            }
            *num = jMaxRate*iMaxPier;
        }
        else
        { // get all j for ix chain
            for (j=0; j<jMaxRate; j++)
            {
                value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT80[ix].tPow2xBase,
                                                    (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT80[ix].tPow2xDelta, j, FALSE,
                                                    ix, CAL_TARGET_POWER_VHT80_INDEX, FALSE));
            }
            *num = jMaxRate;
        }
    }
    else
    {
        if (ix<0 || ix>=iMaxPier)
        {
            for (i=0; i<iMaxPier; i++)
            {
                value[iv++] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xBase,
                                                    (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xDelta, iy, FALSE,
                                                    i, CAL_TARGET_POWER_VHT80_INDEX, FALSE));
                *num = iMaxPier;
            }
        }
        else
        {
            value[0] = (GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT80[ix].tPow2xBase,
                                            (A_INT8*)Qc98xxEepromStructGet()->targetPower5GVHT80[ix].tPow2xDelta, iy, FALSE,
                                            ix, CAL_TARGET_POWER_VHT80_INDEX, FALSE));
            *num = 1;
        }
    }
    return VALUE_OK;
}

int ol_if_ratepwr_usr_to_eeprom(u_int8_t *eep_tbl, u_int16_t eep_len,
                                u_int8_t *usr_tbl, u_int16_t usr_len)
{
    A_INT32 ret = VALUE_OK, num = 0, j = 0;
    QC98XX_EEPROM_RATETBL *pEepromRateTbl = (QC98XX_EEPROM_RATETBL *)eep_tbl;
    QC98XX_USER_TGT_POWER *pUserTgtPower = (QC98XX_USER_TGT_POWER *)usr_tbl;
    pQc9kEepromArea = (A_UINT8*)pEepromRateTbl;

    if (!pEepromRateTbl ||
        !pUserTgtPower ||
        eep_len != sizeof(QC98XX_EEPROM_RATETBL) ||
        usr_len != sizeof(QC98XX_USER_TGT_POWER))
        return ERR_VALUE_BAD;

#ifdef BIG_ENDIAN_HOST
    pEepromRateTbl->validmask = cpu_to_le32(pUserTgtPower->validmask);
#else
    pEepromRateTbl->validmask = pUserTgtPower->validmask;
#endif

    if ( pUserTgtPower->validmask & 0x1 ) //2G valid
    {
        //Convert 2G
        num = OVERLAP_DATA_RATE_CCK * NUM_2G_TGT_POWER_CCK_CHANS;
        ret = Qc98xxCalTGTPwrCCKSet(pUserTgtPower->cck_2G, 0, 0, 0, num, band_BG);

        if ( ret != VALUE_OK )
        {
            goto done;
        }

        num = OVERLAP_DATA_RATE_LEGACY * NUM_2G_TGT_POWER_CHANS;
        ret = Qc98xxCalTGTPwrLegacyOFDMSet(pUserTgtPower->legacy_2G, 0, 0, 0, num, band_BG);

        if ( ret != VALUE_OK )
        {
            goto done;
        }

        for (j = 1; j<= NUM_2G_TGT_POWER_CHANS; j++ )
        {
            num = OVERLAP_DATA_RATE_VHT * j * NUM_STREAMS;
            ret = Qc98xxCalTGTPwrHT20Set(pUserTgtPower->vht20_2G, 0, 0, 0, num, band_BG);

            if ( ret != VALUE_OK )
            {
                goto done;
            }
        }

        for (j = 1; j<= NUM_2G_TGT_POWER_CHANS; j++ )
        {
            num = OVERLAP_DATA_RATE_VHT * j * NUM_STREAMS;
            ret = Qc98xxCalTGTPwrHT40Set(pUserTgtPower->vht40_2G, 0, 0, 0, num, band_BG);

            if ( ret != VALUE_OK )
            {
                goto done;
            }
        }
    }

    if ( pUserTgtPower->validmask & 0x2 ) //5G valid
    {
        //Convert 5G
        num = OVERLAP_DATA_RATE_LEGACY * NUM_5G_TGT_POWER_CHANS;
        ret = Qc98xxCalTGTPwrLegacyOFDMSet(pUserTgtPower->legacy_5G, 0, 0, 0, num, band_A);

        if ( ret != VALUE_OK )
        {
            goto done;
        }

        for (j = 1; j<= NUM_5G_TGT_POWER_CHANS; j++ )
        {
            num = OVERLAP_DATA_RATE_VHT * NUM_STREAMS * j;
            ret = Qc98xxCalTGTPwrHT20Set(pUserTgtPower->vht20_5G, 0, 0, 0, num, band_A);

            if ( ret != VALUE_OK )
            {
                goto done;
            }
        }

        for (j = 1; j<= NUM_5G_TGT_POWER_CHANS; j++ )
        {
            num = OVERLAP_DATA_RATE_VHT * NUM_STREAMS * j;
            ret = Qc98xxCalTGTPwrHT40Set(pUserTgtPower->vht40_5G, 0, 0, 0, num, band_A);

            if ( ret != VALUE_OK )
            {
                goto done;
            }
        }

        for (j = 1; j<= NUM_5G_TGT_POWER_CHANS; j++ )
        {
            num = OVERLAP_DATA_RATE_VHT * NUM_STREAMS * j;
            ret = Qc98xxCalTGTPwrHT80Set(pUserTgtPower->vht80_5G, 0, 0, 0, num, band_A);

            if ( ret != VALUE_OK )
            {
                goto done;
            }
        }
    }

done:
    return ret;
}

int ol_if_ratepwr_eeprom_to_usr(u_int8_t *eep_tbl, u_int16_t eep_len,
                                u_int8_t *usr_tbl, u_int16_t usr_len)
{
    A_INT32 ret = VALUE_OK, num = 0;
    QC98XX_EEPROM_RATETBL *pEepromRateTbl = (QC98XX_EEPROM_RATETBL *)eep_tbl;
    QC98XX_USER_TGT_POWER *pUserTgtPower = (QC98XX_USER_TGT_POWER *)usr_tbl;
    pQc9kEepromArea = (A_UINT8*)pEepromRateTbl;

    if (!pEepromRateTbl ||
        !pUserTgtPower ||
        eep_len != sizeof(QC98XX_EEPROM_RATETBL) ||
        usr_len != sizeof(QC98XX_USER_TGT_POWER))
        return ERR_VALUE_BAD;

    //Convert 2G
    Qc98xxCalTGTPwrCCKGet(pUserTgtPower->cck_2G, -1, -1, 0, &num, band_BG);
    Qc98xxCalTGTPwrLegacyOFDMGet(pUserTgtPower->legacy_2G, -1, -1, 0, &num, band_BG);
    Qc98xxCalTGTPwrHT20Get(pUserTgtPower->vht20_2G, -1, -1, 0, &num, band_BG);
    Qc98xxCalTGTPwrHT40Get(pUserTgtPower->vht40_2G, -1, -1, 0, &num, band_BG);

    //Convert 5G
    Qc98xxCalTGTPwrLegacyOFDMGet(pUserTgtPower->legacy_5G, -1, -1, 0, &num, band_A);
    Qc98xxCalTGTPwrHT20Get(pUserTgtPower->vht20_5G, -1, -1, 0, &num, band_A);
    Qc98xxCalTGTPwrHT40Get(pUserTgtPower->vht40_5G, -1, -1, 0, &num, band_A);
    Qc98xxCalTGTPwrHT80Get(pUserTgtPower->vht80_5G, -1, -1, 0, &num, band_A);

    pUserTgtPower->validmask = pEepromRateTbl->validmask;
    return ret;
}

void ol_if_ratepwr_recv_buf(u_int8_t *ratepwr_tbl, u_int16_t ratepwr_len,
                            u_int8_t frag_id, u_int8_t more_frag)
{
    u_int8_t *bufptr = (u_int8_t *)&eep_ratepwr_tbl;
    u_int16_t max_frag_size;

    /*
     * Assume that:
     * - all frags use max event buffer if it is not the last piece
     * - all frags arrive in order
     */
    if (more_frag)
        max_frag_size = ratepwr_len;
    else {
        if ((sizeof(QC98XX_EEPROM_RATETBL) - ratepwr_len) % frag_id != 0) {
            printk("%s: invalid frag size\n", __func__);
            return;
        }

        max_frag_size = (sizeof(QC98XX_EEPROM_RATETBL) - ratepwr_len) / frag_id;
    }
    bufptr += frag_id * max_frag_size;
    OS_MEMCPY(bufptr, ratepwr_tbl, ratepwr_len);


    if (!more_frag) {
        QC98XX_USER_TGT_POWER usr_tbl;

#ifdef BIG_ENDIAN_HOST
        eep_ratepwr_tbl.validmask = le32_to_cpu(eep_ratepwr_tbl.validmask);
#endif
        if (eep_ratepwr_tbl.validmask == 0)
            return;

        /* perform eep-to-usr format conversion */
        OS_MEMSET(&usr_tbl, 0, sizeof(QC98XX_USER_TGT_POWER));
        ol_if_ratepwr_eeprom_to_usr((u_int8_t *)&eep_ratepwr_tbl, sizeof(QC98XX_EEPROM_RATETBL),
                                    (u_int8_t *)&usr_tbl, sizeof(QC98XX_USER_TGT_POWER));
    }
}

void ol_if_eeprom_attach(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    if (lmac_get_tgt_type(scn->soc->psoc_obj) != TARGET_TYPE_AR9888) {
        qdf_info("rate power table override is only supported for AR98XX");
        return;
    }

    OS_MEMSET(&eep_ratepwr_tbl, 0, sizeof(QC98XX_EEPROM_RATETBL));

    return;
}
