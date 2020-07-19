/*
 * Copyright (c) 2017 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include <athtypes_linux.h>
//
// hal header files
//
#include "qc98xx_eeprom.h"
#include "Qc98xxEepromStructGet.h"
#include "Qc98xxEepromStruct.h"
#include "ParameterConfigDef.h"
#include "rate_constants.h"
#include "vrate_constants.h"
#define TRUE 1
#define FALSE 0


extern int FieldFind(char *name, A_UINT32 *address, int *low, int *high);

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
//	Delta of MCS0_10_20 is stored in tPow2xDelta[3:0]; delta of MCS1_2_11_12_21_22 is stored in tPow2xDelta[7:4], and so on
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
	WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20,				//VHT_TARGET_RATE_0,
	WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22,		//VHT_TARGET_RATE_1_2,
	WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24,		//VHT_TARGET_RATE_3_4,
	WHAL_VHT_TARGET_POWER_RATES_MCS5,					//VHT_TARGET_RATE_5,
	WHAL_VHT_TARGET_POWER_RATES_MCS6,					//VHT_TARGET_RATE_6,
	WHAL_VHT_TARGET_POWER_RATES_MCS7,					//VHT_TARGET_RATE_7,
	WHAL_VHT_TARGET_POWER_RATES_MCS8,					//VHT_TARGET_RATE_8,
	WHAL_VHT_TARGET_POWER_RATES_MCS9,					//VHT_TARGET_RATE_9,
	WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20,				//VHT_TARGET_RATE_10,
	WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22,		//VHT_TARGET_RATE_11_12,
	WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24,		//VHT_TARGET_RATE_13_14,
	WHAL_VHT_TARGET_POWER_RATES_MCS15,					//VHT_TARGET_RATE_15,
	WHAL_VHT_TARGET_POWER_RATES_MCS16,					//VHT_TARGET_RATE_16,
	WHAL_VHT_TARGET_POWER_RATES_MCS17,					//VHT_TARGET_RATE_17,
	WHAL_VHT_TARGET_POWER_RATES_MCS18,					//VHT_TARGET_RATE_18,
	WHAL_VHT_TARGET_POWER_RATES_MCS19,					//VHT_TARGET_RATE_19,
	WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20,				//VHT_TARGET_RATE_20,
	WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22,		//VHT_TARGET_RATE_21_22,
	WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24,		//VHT_TARGET_RATE_23_24,
	WHAL_VHT_TARGET_POWER_RATES_MCS25,					//VHT_TARGET_RATE_25,
	WHAL_VHT_TARGET_POWER_RATES_MCS26,					//VHT_TARGET_RATE_26,
	WHAL_VHT_TARGET_POWER_RATES_MCS27,					//VHT_TARGET_RATE_27,
	WHAL_VHT_TARGET_POWER_RATES_MCS28,					//VHT_TARGET_RATE_28,
	WHAL_VHT_TARGET_POWER_RATES_MCS29,					//VHT_TARGET_RATE_29,
};

int Qc98xxUserRateIndex2Stream (A_UINT16 userRateGroupIndex)
{
    int stream;

    stream = IS_1STREAM_TARGET_POWER_VHT_RATES(userRateGroupIndex) ? 0 :
            (IS_2STREAM_TARGET_POWER_VHT_RATES(userRateGroupIndex) ? 1 :
            (IS_3STREAM_TARGET_POWER_VHT_RATES(userRateGroupIndex) ? 2 : -1));

    return stream;
}


int Qc98xxRateIndex2Stream (A_UINT16 rateIndex)
{
    int stream;

    if (IS_1STREAM_RATE_INDEX(rateIndex) || IS_1STREAM_vRATE_INDEX(rateIndex))
    {
        stream = 0;
    }
    else if (IS_2STREAM_RATE_INDEX(rateIndex) || IS_2STREAM_vRATE_INDEX(rateIndex))
    {
        stream = 1;
    }
    else if (IS_3STREAM_RATE_INDEX(rateIndex) || IS_3STREAM_vRATE_INDEX(rateIndex))
    {
        stream = 2;
    }
    else
    {
        stream = -1;
        printf("Qc98xxRateIndex2Stream - ERROR invalid rate group index\n");
    }
    return stream;
}



static int setFBIN2FREQ(int bin, int iBand)
{
	int freq;
	if (bin==0)
		return 0;
	if (iBand==band_BG)
		freq = WHAL_FBIN2FREQ(bin,1);
	else
		freq = WHAL_FBIN2FREQ(bin,0);
	return freq;
}
static int Interpolate(int x, int *px, int *py, int np)
{
    int ip;
    int lx,ly,lhave;
    int hx = 0, hy = 0, hhave = 0;
    int dx;
    int y;


	lhave=0;
    hhave=0;
    for(ip=0; ip<np; ip++)
    {
        dx=x-px[ip];
        if(dx<=0)
        {
            if(!hhave || dx>(x-hx))
            {
                hx=px[ip];
                hy=py[ip];
                hhave=1;
            }
        }
        if(dx>=0)
        {
            if(!lhave || dx<(x-lx))
            {
                lx=px[ip];
                ly=py[ip];
                lhave=1;
            }
        }
    }
    if(lhave)
    {
        if(hhave)
        {
            if(hx==lx)
            {
                y=ly;
            }
            else
            {
                y=ly+(((x-lx)*(hy-ly))/(hx-lx));
            }
        }
        else
        {
            y=ly;
        }
    }
    else if(hhave)
    {
        y=hy;
    }
    else
    {
        y= -(1<<30);
    }

    return y;
}


QC98XX_EEPROM *Qc98xxEepromStructGet(void)
{
    return (QC98XX_EEPROM *)pQc9kEepromArea;
}

QC98XX_EEPROM *Qc98xxEepromBoardStructGet(void)
{
    return (QC98XX_EEPROM *)pQc9kEepromBoardArea;
}

int Qc98xxEepromCalPierGet(int mode, int ipier, int ichain,
                       int *pfrequency, int *pcorrection, int *ptemperature, int *pvoltage)
{
    return 0;
}

A_UINT8 Qc98xxEepromGetLegacyTrgtPwr(A_UINT16 rateIndex, int freq, A_BOOL is2GHz)
{
    A_UINT16 numPiers, i;
#if (WHAL_NUM_11A_LEG_TARGET_POWER_CHANS > WHAL_NUM_11G_LEG_TARGET_POWER_CHANS)
    A_UINT32 targetPowerArray[WHAL_NUM_11A_LEG_TARGET_POWER_CHANS];
    A_UINT32 freqArray[WHAL_NUM_11A_LEG_TARGET_POWER_CHANS];
#else
    A_UINT32 targetPowerArray[WHAL_NUM_11G_LEG_TARGET_POWER_CHANS];
    A_UINT32 freqArray[WHAL_NUM_11G_LEG_TARGET_POWER_CHANS];
#endif
    CAL_TARGET_POWER_LEG *pEepromTargetPwr;
    A_UINT8 *pFreqBin;

    if (is2GHz) {
        numPiers = WHAL_NUM_11G_LEG_TARGET_POWER_CHANS;
        pEepromTargetPwr = Qc98xxEepromStructGet()->targetPower2G;
        pFreqBin = Qc98xxEepromStructGet()->targetFreqbin2G;
    } else {
        numPiers = WHAL_NUM_11A_LEG_TARGET_POWER_CHANS;
        pEepromTargetPwr = Qc98xxEepromStructGet()->targetPower5G;
        pFreqBin = Qc98xxEepromStructGet()->targetFreqbin5G;
    }

    //create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
        freqArray[i] = WHAL_FBIN2FREQ(pFreqBin[i], is2GHz);
        targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
    }

    //interpolate to get target power for given frequency
    return((A_UINT8)Interpolate(freq, (int *)freqArray, (int *)targetPowerArray, numPiers));
}

A_UINT8 GetHttPow2xFromStreamAndRateIndex(A_UINT8 *tPow2xBase, A_UINT8 *tPow2xDelta, A_UINT8 stream, A_UINT16 rateIndex, A_BOOL byteDelta,
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

// userTargetPowerRateIndex: see TARGET_POWER_HT_RATES in Qc98xxEepromStruct.h
A_UINT8 GetHttPow2x(A_UINT8 *tPow2xBase, A_UINT8 *tPow2xDelta, A_UINT16 userTargetPowerRateIndex, A_BOOL byteDelta,
                    A_UINT16 pierth, A_UINT8 vhtIdx, A_BOOL is2GHz)
{
	A_UINT8 tPow2x;
	int stream;
	A_UINT16 targetPowerRateIndex;

	stream = Qc98xxUserRateIndex2Stream(userTargetPowerRateIndex);
	targetPowerRateIndex = VhtRateIndexToRateGroupTable[userTargetPowerRateIndex];
	if (stream < 0)
	{
		printf ("GetHttPow2x - WARNING: Invalid target power rate %d\n", userTargetPowerRateIndex);
		return 0xff;
	}

	tPow2x = GetHttPow2xFromStreamAndRateIndex(tPow2xBase, tPow2xDelta, stream, targetPowerRateIndex, byteDelta,
                                                    pierth, vhtIdx, is2GHz);
	return tPow2x;
}

A_UINT8 GetVhttPow2xFromRateIndex(A_UINT8 *tPow2xBase, A_UINT8 *tPow2xDelta, A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, A_BOOL byteDelta,
                                  A_UINT16 pierth, A_UINT8 vhtIdx, A_BOOL is2GHz)
{
	A_UINT8 tPow2x;
	int stream;

	tPow2x = 0xff;
	stream = Qc98xxRateIndex2Stream (rateIndex);
	if (stream >= 0)
	{
		tPow2x = GetHttPow2xFromStreamAndRateIndex(tPow2xBase, tPow2xDelta, stream, targetPowerRateIndex, byteDelta,
                                                    pierth, vhtIdx, is2GHz);
	}
	return tPow2x;
}

A_UINT8 Qc98xxEepromGetHT20TrgtPwr2G(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq)
{
    A_UINT16 numPiers, i;
    A_UINT32 targetPowerArray[WHAL_NUM_11G_20_TARGET_POWER_CHANS];
    A_UINT32 freqArray[WHAL_NUM_11G_20_TARGET_POWER_CHANS];
    CAL_TARGET_POWER_11G_20 *pEepromTargetPwr;

    A_UINT8 *pFreqBin;

    numPiers = WHAL_NUM_11G_20_TARGET_POWER_CHANS;
    pEepromTargetPwr = (CAL_TARGET_POWER_11G_20 *)Qc98xxEepromStructGet()->targetPower2GVHT20;
    pFreqBin = Qc98xxEepromStructGet()->targetFreqbin2GVHT20;

    //create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
        freqArray[i] = WHAL_FBIN2FREQ(pFreqBin[i], 1);
		targetPowerArray[i] = GetVhttPow2xFromRateIndex(pEepromTargetPwr[i].tPow2xBase, pEepromTargetPwr[i].tPow2xDelta,
														rateIndex, targetPowerRateIndex, FALSE, i, CAL_TARGET_POWER_VHT20_INDEX, TRUE);
    }

    //interpolate to get target power for given frequency
    return((A_UINT8)Interpolate(freq, (int *)freqArray, (int *)targetPowerArray, numPiers));
}

A_UINT8 Qc98xxEepromGetHT20TrgtPwr5G(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq)
{
    A_UINT16 numPiers, i;
    A_UINT32 targetPowerArray[WHAL_NUM_11A_20_TARGET_POWER_CHANS];
    A_UINT32 freqArray[WHAL_NUM_11A_20_TARGET_POWER_CHANS];
    CAL_TARGET_POWER_11A_20 *pEepromTargetPwr;

    A_UINT8 *pFreqBin;

    numPiers = WHAL_NUM_11A_20_TARGET_POWER_CHANS;
    pEepromTargetPwr = (CAL_TARGET_POWER_11A_20 *)Qc98xxEepromStructGet()->targetPower5GVHT20;
    pFreqBin = Qc98xxEepromStructGet()->targetFreqbin5GVHT20;

    //create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
        freqArray[i] = WHAL_FBIN2FREQ(pFreqBin[i], 0);
		targetPowerArray[i] = GetVhttPow2xFromRateIndex(pEepromTargetPwr[i].tPow2xBase, pEepromTargetPwr[i].tPow2xDelta,
														rateIndex, targetPowerRateIndex, FALSE, i, CAL_TARGET_POWER_VHT20_INDEX, FALSE);
    }

    //interpolate to get target power for given frequency
    return((A_UINT8)Interpolate(freq, (int *)freqArray, (int *)targetPowerArray, numPiers));
}

A_UINT8 Qc98xxEepromGetHT20TrgtPwr(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq, A_BOOL is2GHz)
{
    if (is2GHz)
	{
		return Qc98xxEepromGetHT20TrgtPwr2G(rateIndex, targetPowerRateIndex, freq);
	}
	else
	{
		return Qc98xxEepromGetHT20TrgtPwr5G(rateIndex, targetPowerRateIndex, freq);
	}
}

A_UINT8 Qc98xxEepromGetHT40TrgtPwr2G(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq)
{
    A_UINT16 numPiers, i;
    A_UINT32 targetPowerArray[WHAL_NUM_11G_40_TARGET_POWER_CHANS];
    A_UINT32 freqArray[WHAL_NUM_11G_40_TARGET_POWER_CHANS];
    CAL_TARGET_POWER_11G_40 *pEepromTargetPwr;
    A_UINT8 *pFreqBin;

    numPiers = WHAL_NUM_11G_40_TARGET_POWER_CHANS;
    pEepromTargetPwr = Qc98xxEepromStructGet()->targetPower2GVHT40;
    pFreqBin = Qc98xxEepromStructGet()->targetFreqbin2GVHT40;

	//create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
        freqArray[i] = WHAL_FBIN2FREQ(pFreqBin[i], 1);
		targetPowerArray[i] = GetVhttPow2xFromRateIndex(pEepromTargetPwr[i].tPow2xBase, pEepromTargetPwr[i].tPow2xDelta,
														rateIndex, targetPowerRateIndex, FALSE, i, CAL_TARGET_POWER_VHT40_INDEX, TRUE);
    }

    //interpolate to get target power for given frequency
    return((A_UINT8)Interpolate(freq, (int *)freqArray, (int *)targetPowerArray, numPiers));
}

A_UINT8 Qc98xxEepromGetHT40TrgtPwr5G(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq)
{
    A_UINT16 numPiers, i;
    A_UINT32 targetPowerArray[WHAL_NUM_11A_40_TARGET_POWER_CHANS];
    A_UINT32 freqArray[WHAL_NUM_11A_40_TARGET_POWER_CHANS];
    CAL_TARGET_POWER_11A_40 *pEepromTargetPwr;
    A_UINT8 *pFreqBin;

    numPiers = WHAL_NUM_11A_40_TARGET_POWER_CHANS;
    pEepromTargetPwr = Qc98xxEepromStructGet()->targetPower5GVHT40;
    pFreqBin = Qc98xxEepromStructGet()->targetFreqbin5GVHT40;

	//create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
        freqArray[i] = WHAL_FBIN2FREQ(pFreqBin[i], 0);
        targetPowerArray[i] = GetVhttPow2xFromRateIndex(pEepromTargetPwr[i].tPow2xBase, pEepromTargetPwr[i].tPow2xDelta,
														rateIndex, targetPowerRateIndex, FALSE, i, CAL_TARGET_POWER_VHT40_INDEX, FALSE);
    }

    //interpolate to get target power for given frequency
    return((A_UINT8)Interpolate(freq, (int *)freqArray, (int *)targetPowerArray, numPiers));
}

A_UINT8 Qc98xxEepromGetHT80TrgtPwr5G(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq)
{
    A_UINT16 numPiers, i;
    A_UINT32 targetPowerArray[WHAL_NUM_11A_80_TARGET_POWER_CHANS];
    A_UINT32 freqArray[WHAL_NUM_11A_80_TARGET_POWER_CHANS];
    CAL_TARGET_POWER_11A_80 *pEepromTargetPwr;
    A_UINT8 *pFreqBin;

    numPiers = WHAL_NUM_11A_80_TARGET_POWER_CHANS;
    pEepromTargetPwr = Qc98xxEepromStructGet()->targetPower5GVHT80;
    pFreqBin = Qc98xxEepromStructGet()->targetFreqbin5GVHT80;

	//create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
        freqArray[i] = WHAL_FBIN2FREQ(pFreqBin[i], 0);
        targetPowerArray[i] = GetVhttPow2xFromRateIndex(pEepromTargetPwr[i].tPow2xBase, pEepromTargetPwr[i].tPow2xDelta,
														rateIndex, targetPowerRateIndex, FALSE, i, CAL_TARGET_POWER_VHT80_INDEX, FALSE);
    }

    //interpolate to get target power for given frequency
    return((A_UINT8)Interpolate(freq, (int *)freqArray, (int *)targetPowerArray, numPiers));
}

A_UINT8 Qc98xxEepromGetHT40TrgtPwr(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq, A_BOOL is2GHz)
{
    if (is2GHz)
	{
		return Qc98xxEepromGetHT40TrgtPwr2G(rateIndex, targetPowerRateIndex, freq);
    }
	else
	{
		return Qc98xxEepromGetHT40TrgtPwr5G(rateIndex, targetPowerRateIndex, freq);
    }
}

A_UINT8 Qc98xxEepromGetHT80TrgtPwr(A_UINT16 rateIndex, A_UINT16 targetPowerRateIndex, int freq, A_BOOL is2GHz)
{
    if (is2GHz)
	{
		return 0;
    }
	else
	{
		return Qc98xxEepromGetHT80TrgtPwr5G(rateIndex, targetPowerRateIndex, freq);
    }
}

A_UINT8 Qc98xxEepromGetCckTrgtPwr(A_UINT16 rateIndex, int freq)
{
    A_UINT16 numPiers = WHAL_NUM_11B_TARGET_POWER_CHANS, i;
    A_UINT32 targetPowerArray[WHAL_NUM_11B_TARGET_POWER_CHANS];
    A_UINT32 freqArray[WHAL_NUM_11B_TARGET_POWER_CHANS];
    CAL_TARGET_POWER_11B *pEepromTargetPwr = Qc98xxEepromStructGet()->targetPowerCck;
    A_UINT8 *pFreqBin = Qc98xxEepromStructGet()->targetFreqbinCck;

    //create array of channels and targetpower from targetpower piers stored on eeprom
    for (i = 0; i < numPiers; i++) {
        freqArray[i] = WHAL_FBIN2FREQ(pFreqBin[i], 1);
        targetPowerArray[i] = pEepromTargetPwr[i].tPow2x[rateIndex];
    }

    //interpolate to get target power for given frequency
    return((A_UINT8)Interpolate(freq, (int *)freqArray, (int *)targetPowerArray, numPiers));
}

int Qc98xxEepromVersionGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.eeprom_version;
    return value;
}

int Qc98xxTemplateVersionGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.template_version;
    return value;
}

/*
 *Function Name:Qc98xxPwrTuningCapsParamsGet
 *Parameters: returned string value for get 2 uint8 value delim with ,
 *Description: Get TuningCapsParams values from field of eeprom struct 2 uint8
 *Returns: zero
 */
int Qc98xxPwrTuningCapsParamsGet(int *value, int ix, int *num)
{
    if (ix<0 || ix>2)
    {
		value[0] = Qc98xxEepromStructGet()->baseEepHeader.param_for_tuning_caps;
		value[1] = Qc98xxEepromStructGet()->baseEepHeader.param_for_tuning_caps_1;
		*num = 2;
	}
    else if (ix == 0)
    {
		value[0] = Qc98xxEepromStructGet()->baseEepHeader.param_for_tuning_caps;
		*num = 1;
	}
    else
    {
		value[0] = Qc98xxEepromStructGet()->baseEepHeader.param_for_tuning_caps_1;
		*num = 1;
    }
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxRegDmnGet
 *Parameters: returned string value for get value
 *Description: Get regDmn value from field of eeprom struct uint16*2
 *Returns: zero
 */
int Qc98xxRegDmnGet(int *value, int ix, int *num)
{
	if (ix<0 || ix>2) {
		value[0] = Qc98xxEepromStructGet()->baseEepHeader.regDmn[0];
		value[1] = Qc98xxEepromStructGet()->baseEepHeader.regDmn[1];
		*num = 2;
	} else {
		value[0] = Qc98xxEepromStructGet()->baseEepHeader.regDmn[1];
		*num = 1;
	}
    return VALUE_OK;
}

int Qc98xxRegulatoryDomainGet(void)
{
	return Qc98xxEepromStructGet()->baseEepHeader.regDmn[0];
}

int Qc98xxRegulatoryDomain1Get(void)
{
	return Qc98xxEepromStructGet()->baseEepHeader.regDmn[1];
}


int Qc98xxMacAddressGet(unsigned char mac[6])
{
    QC98XX_EEPROM *pEepData = (QC98XX_EEPROM *)Qc98xxEepromArea;

    // Read the mac address from the eeprom
    memcpy (mac, pEepData->baseEepHeader.macAddr, sizeof(pEepData->baseEepHeader.macAddr));
    return 0;
}

/*
 *Function Name:Qc98xxTxRxMaskGet
 *Parameters: returned string value for get value
 *Description: Get txMask value from field of eeprom struct uint8 (up 4 bit???)
 *Returns: zero
 */
int Qc98xxTxRxMaskGet(void)
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.txrxMask;
    return value;
}

/*
 *Function Name:Qc98xxTxMaskGet
 *Parameters: returned string value for get value
 *Description: Get txMask value from field of eeprom struct uint8 (up 4 bit???)
 *Returns: zero
 */
int Qc98xxTxMaskGet(void)
{
	A_UINT8  value;
	value = (Qc98xxEepromStructGet()->baseEepHeader.txrxMask & 0xf0) >> 4;
    return value;
}

/*
 *Function Name:Qc98xxRxMaskGet
 *Parameters: returned string value for get value
 *Description: Get rxMask value from field of eeprom struct uint8 (low 4bits??)
 *Returns: zero
 */
int Qc98xxRxMaskGet(void)
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.txrxMask & 0x0f;
    return value;
}
/*
 *Function Name:Qc98xxOpFlagsGet
 *Parameters: returned string value for get value
 *Description: Get opFlags value from field of eeprom struct uint8
 *Returns: zero
 */
int Qc98xxOpFlagsGet(void)
{
	A_UINT32  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.opFlags;
    return value;
}

/*
 *Function Name:Qc98xxOpFlags2Get
 *Parameters: returned string value for get value
 *Description: Get opFlags value from field of eeprom struct uint8
 *Returns: zero
 */
int Qc98xxOpFlags2Get(void)
{
	A_UINT32  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.opFlags2;
    return value;
}

/*
 *Function Name:Qc98xxEepMiscGet
 *Parameters: returned string value for get value
 *Description: Get eepMisc value from field of eeprom struct uint8
 *Returns: zero
 */
int Qc98xxEepMiscGet()
{
	A_UINT32  value;
	value = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.miscConfiguration & WHAL_MISCCONFIG_EEPMISC_MASK) >>
				WHAL_MISCCONFIG_EEPMISC_SHIFT;
    return value;
}/*
 *Function Name:Qc98xxRfSilentGet
 *Parameters: returned string value for get value
 *Description: Get rfSilent value from field of eeprom struct uint8
 *Returns: zero
 */
int Qc98xxRfSilentGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.rfSilent;
    return value;
}

int Qc98xxRfSilentB0Get()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.rfSilent & 0x01;
    return value;
}
int Qc98xxRfSilentB1Get()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.rfSilent >>1;
    return value;
}
int Qc98xxRfSilentGPIOGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.rfSilent>>2;
    return value;
}

/*
 *Function Name:Qc98xxBlueToothOptionsGet
 *Parameters: returned string value for get value
 *Description: Get blueToothOptions value from field of eeprom struct uint8
 *Returns: zero
 */
int Qc98xxBlueToothOptionsGet()
{
	A_UINT16  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.blueToothOptions;
    return value;
}

#if 0
/*
 *Function Name:Qc98xxDeviceCapGet
 *Parameters: returned string value for get value
 *Description: Get deviceCap value from field of eeprom struct uint8
 *Returns: zero
 */
int Qc98xxDeviceCapGet(char *sValue)
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.deviceCap;
    return value;
}
/*
 *Function Name:Qc98xxDeviceTypeGet
 *Parameters: returned string value for get value
 *Description: Get deviceType value from field of eeprom struct uint8
 *Returns: zero
 */
int Qc98xxDeviceTypeGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.deviceType;
    return value;
}
#endif //0

/*
 *Function Name:Qc98xxPwrTableOffsetGet
 *Parameters: returned string value for get value
 *Description: Get pwrTableOffset value from field of eeprom struct int8
 *Returns: zero
 */
int Qc98xxPwrTableOffsetGet()
{
	A_INT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.pwrTableOffset;
    return value;
}

#if 0
/*
 *Function Name:Qc98xxTempSlopeGet
 *Parameters: returned string value for get value
 *Description: Get TempSlope value from field of eeprom struct int8
 *Returns: zero
 */
int Qc98xxTempSlopeGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].tempSlope[i];
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].tempSlope[i];
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].tempSlope[ix];
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].tempSlope[ix];
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

int Qc98xxTempSlopeLowGet(int *value)
{
	value[0] = Qc98xxEepromStructGet()->baseEepHeader.temp_slope_low;
    return VALUE_OK;
}

int Qc98xxTempSlopeHighGet(int *value)
{
	value[0] = Qc98xxEepromStructGet()->baseEepHeader.temp_slope_high;
    return VALUE_OK;
}
#endif //0

/*
 *Function Name:Qc98xxVoltSlopeGet
 *Parameters: returned string value for get value
 *Description: Get VoltSlope value from field of eeprom struct int8
 *Returns: zero
 */
int Qc98xxVoltSlopeGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>=iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].voltSlope[i];
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].voltSlope[i];
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].voltSlope[ix];
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].voltSlope[ix];
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxReconfigMiscGet
 *Description: Get miscConfiguration all bits
 */
int Qc98xxReconfigMiscGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.miscConfiguration;
    return value;
}

/*
 *Function Name:Qc98xxReconfigDriveStrengthGet
 *
 *Parameters: returned string value for get bit
 *
 *Description: Get reconfigDriveStrength flag from miscConfiguration
 *             field of eeprom struct (bit 0)
 *
 *Returns: zero
 *
 */
int Qc98xxReconfigDriveStrengthGet()
{
	A_UINT8  bit;
	bit = Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.miscConfiguration & WHAL_MISCCONFIG_DRIVE_STRENGTH;
    return bit;
}

int Qc98xxThermometerGet()
{
	A_INT8 thermometer = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.miscConfiguration >> 1)& 0x3;
	thermometer--;
	return thermometer;
}

int Qc98xxChainMaskReduceGet()
{
	A_UINT8 value = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.miscConfiguration >> 3)& 0x1;
	return value;
}

int Qc98xxReconfigQuickDropGet()
{
	A_UINT8  bit;
	bit = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.miscConfiguration >> 4) & 1;
    return bit;
}

int Qc98xxWlanLedGpioGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.wlanLedGpio ;
    return value;
}

int Qc98xxSpurBaseAGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.spurBaseA ;
    return value;
}

int Qc98xxSpurBaseBGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.spurBaseB;
    return value;
}

int Qc98xxSpurRssiThreshGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.spurRssiThresh;
    return value;
}

int Qc98xxSpurRssiThreshCckGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.spurRssiThreshCck;
    return value;
}

int Qc98xxSpurMitFlagGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.spurMitFlag;
    return value;
}

#if 0
int Qc98xxEepromWriteEnableGpioGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.eepromWriteEnableGpio ;
    return value;
}

int Qc98xxWlanDisableGpioGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.wlanDisableGpio ;
    return value;
}


int Qc98xxRxBandSelectGpioGet()
{
	A_UINT8  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.rxBandSelectGpio ;
    return value;
}
#endif //0

int Qc98xxTxRxGainGet(int iBand)
{
	A_UINT8  value;

    if (Qc98xxEepromStructGet()->baseEepHeader.eeprom_version == QC98XX_EEP_VER1)
    {
	    value = Qc98xxEepromStructGet()->baseEepHeader.txrxgain;
    }
    else
    {
        if (iBand == band_both || iBand == band_BG)
        {
	        value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txrxgain;
        }
        else
        {
	        value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txrxgain;
        }
    }
    return value;
}
/*
 *Function Name:Qc98xxTxGainGet
 *Parameters: returned string value for get bit
 *Description: Get TxGain flag in txrxgain field of eeprom struct's upper 4bits
 *Returns: zero
 */
int Qc98xxTxGainGet(int iBand)
{
	A_UINT8  value;

    if (Qc98xxEepromStructGet()->baseEepHeader.eeprom_version == QC98XX_EEP_VER1)
    {
	    value = (Qc98xxEepromStructGet()->baseEepHeader.txrxgain  & 0xf0) >> 4;
    }
    else
    {
        if (iBand == band_both || iBand == band_BG)
        {
	        value = (Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txrxgain & 0xf0) >> 4;
        }
        else
        {
	        value = (Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txrxgain & 0xf0) >> 4;
        }
    }
    return value;
}
/*
 *Function Name:Qc98xxRxGainGet
 *Parameters: returned string value for get bit
 *Description: Get RxGain flag in txrxgain field of eeprom struct's upper 4bits
 *Returns: zero
 */
int Qc98xxRxGainGet(int iBand)
{
	A_UINT8  value;

    if (Qc98xxEepromStructGet()->baseEepHeader.eeprom_version == QC98XX_EEP_VER1)
    {
	    value = Qc98xxEepromStructGet()->baseEepHeader.txrxgain  & 0x0f;
    }
    else
    {
        if (iBand == band_both || iBand == band_BG)
        {
	        value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txrxgain & 0x0f;
        }
        else
        {
	        value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txrxgain & 0x0f;
        }
    }
    return value;
}

int Qc98xxSWREGGet()
{
    A_UINT32 value;
    value = Qc98xxEepromStructGet()->baseEepHeader.swreg;
    return value;
}

/*
 *Function Name:Qc98xxEnableFeatureGet
 *Description: get all featureEnable 8bits
 *             field of eeprom struct (bit 0)
 * enable tx temp comp
 */
int Qc98xxEnableFeatureGet()
{
	int value;
    value = Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.featureEnable;
    return value;
}

/*
 *Function Name:Qc98xxEnableTempCompensationGet
 *
 *Parameters: returned string value for get bit
 *
 *Description: get reconfigDriveStrength flag from featureEnable
 *             field of eeprom struct (bit 0)
 *
 *Returns: zero
 *
 */
int Qc98xxEnableTempCompensationGet()
{
	A_UINT8 value;
    value = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.featureEnable & WHAL_FEATUREENABLE_TEMP_COMP_MASK) >>
			WHAL_FEATUREENABLE_TEMP_COMP_SHIFT;
    return value;
}

/*
 *Function Name:Qc98xxEnableVoltCompensationGet
 *
 *Parameters: returned string value for get bit
 *
 *Description: get reconfigDriveStrength flag from featureEnable
 *             field of eeprom struct (bit 1)
 *
 *Returns: zero
 *
 */
int Qc98xxEnableVoltCompensationGet()
{
	A_UINT8 value;
    value = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.featureEnable & WHAL_FEATUREENABLE_VOLT_COMP_MASK) >>
			WHAL_FEATUREENABLE_VOLT_COMP_SHIFT;
    return value;
}

#if 0
/*
 *Function Name:Qc98xxEnableFastClockGet
 *
 *Parameters: returned string value for get bit
 *
 *Description: Get reconfigDriveStrength flag from featureEnable
 *             field of eeprom struct (bit 2)
 *
 *Returns: zero
 *
 */
int Qc98xxEnableFastClockGet()
{
	A_UINT8 value;
    value = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.featureEnable & 0x04) >> 2;
    return value;
}
#endif //0

/*
 *Function Name:Qc98xxEnableDoublingGet
 *
 *Parameters: returned string value for get bit
 *
 *Description: get reconfigDriveStrength flag from featureEnable
 *             field of eeprom struct (bit 3)
 *
 *Returns: zero
 *
 */
int Qc98xxEnableDoublingGet()
{
	A_UINT8 value;
    value = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.featureEnable & WHAL_FEATUREENABLE_DOUBLING_MASK) >>
			WHAL_FEATUREENABLE_DOUBLING_SHIFT;
    return value;
}

/*
 *Function Name:Qc98xxInternalRegulatorGet
 *
 *Parameters: returned string value for get bit
 *
 *Description: get internal regulator flag from featureEnable
 *             field of eeprom struct (bit 4)
 *
 *Returns: zero
 *
 */
int Qc98xxInternalRegulatorGet()
{
	A_UINT8 value;
    value = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.featureEnable & WHAL_FEATUREENABLE_INTERNAL_REGULATOR_MASK) >>
			WHAL_FEATUREENABLE_INTERNAL_REGULATOR_SHIFT;
    return value;
}

int Qc98xxBoardFlagsGet(void)
{
	A_UINT32  value;
	value = Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.boardFlags;
    return value;
}

/*
 *Function Name:Qc98xxPapdGet
 *
 *Parameters: returned string value for get bit
 *
 *Description: get PA predistortion enable flag from featureEnable
 *             field of eeprom struct (bit 5)
 *
 *Returns: zero
 *
 */
#ifndef WHAL_BOARD_PAPRD_2G_DISABLE
#define WHAL_BOARD_USE_OTP_XTALCAPS	0x040
#define WHAL_BOARD_PAPRD_2G_DISABLE 0x2000
#define WHAL_BOARD_PAPRD_5G_DISABLE 0x4000
#endif

int Qc98xxRbiasGet(void)
{
	A_UINT32 value;

    value = Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.boardFlags & WHAL_BOARD_USE_OTP_XTALCAPS;
    return (value == 0 ? 0 : 1);
}

int Qc98xxPapdGet(void)
{
	A_UINT32 value;

    value = Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.boardFlags &
            (WHAL_BOARD_PAPRD_2G_DISABLE | WHAL_BOARD_PAPRD_5G_DISABLE);
    if (value == 0)
        value = 1; //PAPRD enabled
    else
        value = 0; //PAPRD disable

    return value;
}

/*
 *Function Name:Qc98xxEnableTuningCapsGet
 *Description: get EnableTuningCaps flag from featureEnable
 *             field of eeprom struct (bit 6)
 * enable TuningCaps - default to 0
 */
int Qc98xxEnableTuningCapsGet()
{
	int value;
    value = (Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.featureEnable & WHAL_FEATUREENABLE_TUNING_CAPS_MASK) >> WHAL_FEATUREENABLE_TUNING_CAPS_SHIFT;
    return value;
}

int Qc98xxDeltaCck20Get()
{
	int value;
    value = (int)(Qc98xxEepromStructGet()->baseEepHeader.deltaCck20_t10 / 10);
    return value;
}

int Qc98xxDelta4020Get()
{
	int value;
    value = (int)(Qc98xxEepromStructGet()->baseEepHeader.delta4020_t10 / 10);
    return value;
}

int Qc98xxDelta8020Get()
{
	int value;
    value = (int)(Qc98xxEepromStructGet()->baseEepHeader.delta8020_t10 / 10);
    return value;
}

/*
 *Function Name:Qc98xxCustomerDataGet
 *
 *Parameters: data -- pointer to output array.
 *            len -- size of output array.
 *
 *Description: Returns customer data array from eeprom structure.
 *
 *Returns: -1 on error condition
 *          0 on success.
 *
 */

A_INT32 Qc98xxCustomerDataGet(A_UCHAR *data, int maxlength)
{
    int i;
	int length;

    length = (maxlength>CUSTOMER_DATA_SIZE) ? CUSTOMER_DATA_SIZE : maxlength;

    for(i=0; i<length; i++)
	{
        data[i] = Qc98xxEepromStructGet()->baseEepHeader.custData[i];
	}
	for(i=length; i<maxlength; i++)
	{
		data[i]=0;
	}

    return 0;
}

A_INT32 Qc98xxCalTgtPwrGet(int *pwrArr, int band, int htMode, int iFreqNum)
{
	int i, j, iMax, jMax;

	if (band==band_BG)
	{
		if (htMode==legacy_CCK)
		{
			for (i=0; i<WHAL_NUM_11B_TARGET_POWER_CHANS;i++)
			{
				if (iFreqNum ==i)
				{
					for (j=0; j<WHAL_NUM_LEGACY_TARGET_POWER_RATES; j++)
					{
						pwrArr[j] = (int)(Qc98xxEepromStructGet()->targetPowerCck[i].tPow2x[j]/2);
					}
					break;
				}
			}
		}
		else
		{
			iMax = (htMode==legacy_OFDM) ? WHAL_NUM_11G_LEG_TARGET_POWER_CHANS :
					(htMode==HT20 ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11G_40_TARGET_POWER_CHANS);
			jMax = (htMode==legacy_OFDM) ? WHAL_NUM_LEGACY_TARGET_POWER_RATES :
					(htMode==HT20 ? WHAL_NUM_11G_20_TARGET_POWER_RATES : WHAL_NUM_11G_40_TARGET_POWER_RATES);

			for (i=0; i<iMax;i++)
			{
				if (iFreqNum ==i)
				{
					if (htMode==legacy_OFDM)
					{
						for (j=0; j<jMax; j++)
						{
							pwrArr[j] = (int)(Qc98xxEepromStructGet()->targetPower2G[i].tPow2x[j]/2);
						}
					}
					else if (htMode==HT20)
					{
						for (j=0; j<jMax; j++)
						{
							pwrArr[j] = (int)((GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xDelta, j, FALSE,
                                                        i, CAL_TARGET_POWER_VHT20_INDEX, TRUE)) / 2);
						}
					}
					else if (htMode==HT40)
					{
						for (j=0; j<jMax; j++)
						{
							pwrArr[j] = (int)((GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xDelta, j, FALSE,
                                                        i, CAL_TARGET_POWER_VHT40_INDEX, TRUE)) / 2);
						}
					}
					break;
				}
			}
		}
	}

	else if (band==band_A)
	{
		iMax = (htMode==legacy_OFDM) ? WHAL_NUM_11A_LEG_TARGET_POWER_CHANS :
				(htMode==HT20 ? WHAL_NUM_11A_20_TARGET_POWER_CHANS :
				(htMode==HT40 ? WHAL_NUM_11A_40_TARGET_POWER_CHANS : WHAL_NUM_11A_80_TARGET_POWER_CHANS));
		jMax = (htMode==legacy_OFDM) ? WHAL_NUM_LEGACY_TARGET_POWER_RATES :
				(htMode==HT20 ? WHAL_NUM_11A_20_TARGET_POWER_CHANS :
					(htMode==HT40 ? WHAL_NUM_11A_40_TARGET_POWER_CHANS : WHAL_NUM_11A_80_TARGET_POWER_CHANS));


		for (i=0; i<iMax;i++)
		{
			if (iFreqNum ==i)
			{
				if (htMode==legacy_OFDM)
				{
					for (j=0; j<jMax; j++)
					{
						pwrArr[j] = (int)(Qc98xxEepromStructGet()->targetPower5G[i].tPow2x[j]/2);
					}
				}
				else if (htMode==HT20)
				{
					for (j=0; j<jMax; j++)
					{
						pwrArr[j] = (int)((GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xDelta, j, FALSE,
                                                        i, CAL_TARGET_POWER_VHT20_INDEX, FALSE)) / 2);
					}
				}
				else if (htMode==HT40)
				{
					for (j=0; j<jMax; j++)
					{
						pwrArr[j] = (int)((GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xDelta, j, FALSE,
                                                        i, CAL_TARGET_POWER_VHT40_INDEX, FALSE)) / 2);
					}
				}
				else if (htMode==HT80)
				{
					for (j=0; j<jMax; j++)
					{
						pwrArr[j] = (int)((GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xDelta, j, FALSE,
                                                        i, CAL_TARGET_POWER_VHT80_INDEX, FALSE)) / 2);
					}
				}
				break;
			}
		}
	}
	return 0;
}

A_INT32 Qc98xxCalTgtFreqGet(int *freqArr, int band, int htMode)
{
	int i;

	if (band==band_BG)
	{
		if (htMode==legacy_CCK)
		{
			for (i=0; i<WHAL_NUM_11B_TARGET_POWER_CHANS;i++)
			{
                freqArr[i] = (int)Qc98xxEepromStructGet()->targetFreqbinCck[i];
				freqArr[i] = setFBIN2FREQ(freqArr[i],band_BG);
			}
		}
		else if (htMode==legacy_OFDM)
		{
			for (i=0; i<WHAL_NUM_11G_20_TARGET_POWER_CHANS;i++)
			{
                freqArr[i] = (int)Qc98xxEepromStructGet()->targetFreqbin2G[i];
				freqArr[i] = setFBIN2FREQ(freqArr[i],band_BG);
			}
		}
		else if (htMode==HT20)
		{
			for (i=0; i<WHAL_NUM_11G_20_TARGET_POWER_CHANS;i++)
			{
                freqArr[i] = (int)Qc98xxEepromStructGet()->targetFreqbin2GVHT20[i];
				freqArr[i] = setFBIN2FREQ(freqArr[i],band_BG);
			}
		}
		else if (htMode==HT40)
		{
			for (i=0; i<WHAL_NUM_11G_40_TARGET_POWER_CHANS;i++)
			{
				freqArr[i] = (int)Qc98xxEepromStructGet()->targetFreqbin2GVHT40[i];
				freqArr[i] = setFBIN2FREQ(freqArr[i],band_BG);
			}
		}
	}
	else if (band==band_A)
	{
		if (htMode==legacy_OFDM)
		{
			for (i=0; i<WHAL_NUM_11A_20_TARGET_POWER_CHANS;i++)
			{
                freqArr[i] = (int)Qc98xxEepromStructGet()->targetFreqbin5G[i];
				freqArr[i] = setFBIN2FREQ(freqArr[i],band_A);
			}
		}
		else if (htMode==HT20)
		{
			for (i=0; i<WHAL_NUM_11A_20_TARGET_POWER_CHANS;i++)
			{
				freqArr[i] = (int)Qc98xxEepromStructGet()->targetFreqbin5GVHT20[i];
				freqArr[i] = setFBIN2FREQ(freqArr[i],band_A);
			}
		}
		else if (htMode==HT40)
		{
			for (i=0; i<WHAL_NUM_11A_40_TARGET_POWER_CHANS;i++) {
				freqArr[i] = (int)Qc98xxEepromStructGet()->targetFreqbin5GVHT40[i];
				freqArr[i] = setFBIN2FREQ(freqArr[i],band_A);
			}
		}
		else if (htMode==HT80)
		{
			for (i=0; i<WHAL_NUM_11A_80_TARGET_POWER_CHANS;i++)
			{
				freqArr[i] = (int)Qc98xxEepromStructGet()->targetFreqbin5GVHT80[i];
				freqArr[i] = setFBIN2FREQ(freqArr[i],band_A);
			}
		}
	}
	return 0;
}
/*
 *Function Name:Qc98xxAntCtrlChainGet
 *Parameters: returned string value for get value
 *Description: Get antCtrlChain flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int16_t)
 *Returns: zero
 */
A_INT32 Qc98xxAntCtrlChainGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>=iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].antCtrlChain[i];
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].antCtrlChain[i];
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].antCtrlChain[ix];
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].antCtrlChain[ix];
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxAntCtrlCommonGet
 *Parameters: returned string value for get value
 *Description: Get AntCtrlCommon value from field of eeprom struct u_int32
 *Returns: zero
 */
int Qc98xxAntCtrlCommonGet(int iBand)
{
	A_UINT32  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].antCtrlCommon;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].antCtrlCommon;
	}
    return value;
}
/*
 *Function Name:Qc98xxAntCtrlCommon2Get
 *Parameters: returned string value for get value
 *Description: Get AntCtrlCommon2 value from field of eeprom struct u_int32
 *Returns: zero
 */
int Qc98xxAntCtrlCommon2Get(int iBand)
{
	A_UINT32  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].antCtrlCommon2;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].antCtrlCommon2;
	}
    return value;
}

int Qc98xxRxFilterCapGet(int iBand)
{
	A_UINT32  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].rxFilterCap;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].rxFilterCap;
	}
    return value;
}

int Qc98xxRxGainCapGet(int iBand)
{
	A_UINT32  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].rxGainCap;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].rxGainCap;
	}
    return value;
}

/*
 *Function Name:Qc98xxXatten1DBGet
 *Parameters: returned string value for get value
 *Description: Get xatten1DB flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxXatten1DBGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>=iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->freqModalHeader.xatten1DB[i].value2G;
			} else {
				val = Qc98xxEepromStructGet()->freqModalHeader.xatten1DB[i].value5GMid;
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->freqModalHeader.xatten1DB[ix].value2G;
		} else {
			val = Qc98xxEepromStructGet()->freqModalHeader.xatten1DB[ix].value5GMid;
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxXatten1MarginGet
 *Parameters: returned string value for get value
 *Description: Get xatten1Margin flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxXatten1MarginGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>=iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->freqModalHeader.xatten1Margin[i].value2G;
			} else {
				val = Qc98xxEepromStructGet()->freqModalHeader.xatten1Margin[i].value5GMid;
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->freqModalHeader.xatten1Margin[ix].value2G;
		} else {
			val = Qc98xxEepromStructGet()->freqModalHeader.xatten1Margin[ix].value5GMid;
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

#if 0
/*
 *Function Name:Qc98xxXatten1HystGet
 *Parameters: returned string value for get value
 *Description: Get xatten1Hyst flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxXatten1HystGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].xatten1Hyst[i];
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].xatten1Hyst[i];
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].xatten1Hyst[ix];
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].xatten1Hyst[ix];
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxXatten2DBGet
 *Parameters: returned string value for get value
 *Description: Get xatten2DB flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxXatten2DBGet(int *value, int ix, int *num, int iBand)
{
	int i, iv=0, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].xatten2Db[i];
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].xatten2Db[i];
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].xatten2Db[ix];
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].xatten2Db[ix];
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxXatten2MarginGet
 *Parameters: returned string value for get value
 *Description: Get xatten2Margin flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxXatten2MarginGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].xatten2Margin[i];
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].xatten2Margin[i];
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].xatten2Margin[ix];
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].xatten2Margin[ix];
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxXatten2HystGet
 *Parameters: returned string value for get value
 *Description: Get xatten2Hyst flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxXatten2HystGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].xatten2Hyst[i];
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].xatten2Hyst[i];
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].xatten2Hyst[ix];
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].xatten2Hyst[ix];
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}
#endif //0

A_INT32 Qc98xxXatten1DBLowGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	if (iBand==band_BG)
    {
		*num=0;
		return 0;
	}
	if (ix<0 || ix>=iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			value[i] = Qc98xxEepromStructGet()->freqModalHeader.xatten1DB[i].value5GLow;
		}
		*num = iCMaxChain;
	}
    else
    {
		value[0] = Qc98xxEepromStructGet()->freqModalHeader.xatten1DB[ix].value5GLow;
		*num = 1;
	}
    return VALUE_OK;
}

A_INT32 Qc98xxXatten1DBHighGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	if (iBand==band_BG)
    {
		*num=0;
		return 0;
	}
	if (ix<0 || ix>=iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			value[i] = Qc98xxEepromStructGet()->freqModalHeader.xatten1DB[i].value5GHigh;
		}
		*num = iCMaxChain;
	}
    else
    {
		value[0] = Qc98xxEepromStructGet()->freqModalHeader.xatten1DB[ix].value5GHigh;
		*num = 1;
	}
    return VALUE_OK;
}

A_INT32 Qc98xxXatten1MarginLowGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	if (iBand==band_BG)
    {
		*num=0;
		return 0;
	}
	if (ix<0 || ix>=iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			value[i] = Qc98xxEepromStructGet()->freqModalHeader.xatten1Margin[i].value5GLow;
		}
		*num = iCMaxChain;
	}
    else
    {
		value[0] = Qc98xxEepromStructGet()->freqModalHeader.xatten1Margin[ix].value5GLow;
		*num = 1;
	}
    return VALUE_OK;
}

A_INT32 Qc98xxXatten1MarginHighGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	if (iBand==band_BG)
    {
		*num=0;
		return 0;
	}
	if (ix<0 || ix>=iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			value[i] = Qc98xxEepromStructGet()->freqModalHeader.xatten1Margin[i].value5GHigh;
		}
		*num = iCMaxChain;
	}
    else
    {
		value[0] = Qc98xxEepromStructGet()->freqModalHeader.xatten1Margin[ix].value5GHigh;
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxSpurChansGet
 *Parameters: returned string value for get value
 *Description: Get spurChans flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxSpurChansGet(int *value, int ix, int *num, int iBand)
{
	int i;
	int val;
	if (ix<0 || ix>=QC98XX_EEPROM_MODAL_SPURS)
    {
		for (i=0; i<QC98XX_EEPROM_MODAL_SPURS; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].spurChans[i].spurChan;
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].spurChans[i].spurChan;
			}
			value[i] = setFBIN2FREQ(val, iBand);
		}
		*num = QC98XX_EEPROM_MODAL_SPURS;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].spurChans[ix].spurChan;
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].spurChans[ix].spurChan;
		}
		value[0] = setFBIN2FREQ(val, iBand);
		*num = 1;
	}
    return VALUE_OK;
}

A_INT32 Qc98xxSpurAPrimSecChooseGet(int *value, int ix, int *num, int iBand)
{
	int i;
	int val;
	if (ix<0 || ix>=QC98XX_EEPROM_MODAL_SPURS)
    {
		for (i=0; i<QC98XX_EEPROM_MODAL_SPURS; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].spurChans[i].spurA_PrimSecChoose;
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].spurChans[i].spurA_PrimSecChoose;
			}
			value[i] = setFBIN2FREQ(val, iBand);
		}
		*num = QC98XX_EEPROM_MODAL_SPURS;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].spurChans[ix].spurA_PrimSecChoose;
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].spurChans[ix].spurA_PrimSecChoose;
		}
		value[0] = setFBIN2FREQ(val, iBand);
		*num = 1;
	}
    return VALUE_OK;
}

A_INT32 Qc98xxSpurBPrimSecChooseGet(int *value, int ix, int *num, int iBand)
{
	int i;
	int val;
	if (ix<0 || ix>=QC98XX_EEPROM_MODAL_SPURS)
    {
		for (i=0; i<QC98XX_EEPROM_MODAL_SPURS; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].spurChans[i].spurB_PrimSecChoose;
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].spurChans[i].spurB_PrimSecChoose;
			}
			value[i] = setFBIN2FREQ(val, iBand);
		}
		*num = QC98XX_EEPROM_MODAL_SPURS;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].spurChans[ix].spurB_PrimSecChoose;
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].spurChans[ix].spurB_PrimSecChoose;
		}
		value[0] = setFBIN2FREQ(val, iBand);
		*num = 1;
	}
    return VALUE_OK;
}

#if 0
/*
 *Function Name:Qc98xxNoiseFloorThreshChGet
 *Parameters: returned string value for get value
 *Description: Get noiseFloorThreshCh flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxNoiseFloorThreshChGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].noiseFloorThreshCh[i];
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].noiseFloorThreshCh[i];
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].noiseFloorThreshCh[ix];
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].noiseFloorThreshCh[ix];
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxObGet
 *Parameters: returned string value for get value
 *Description: Get ob flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxObGet(int *value, int ix, int *num, int iBand)
{
	int i, iCMaxChain=WHAL_NUM_CHAINS;
	int val;
	if (ix<0 || ix>iCMaxChain)
    {
		for (i=0; i<iCMaxChain; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].ob_db[i].ob_dbValue;
			} else {
				val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].ob_db[i].ob_dbValue;
			}
			value[i] = val;
		}
		*num = iCMaxChain;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].ob_db[ix].ob_dbValue;
		} else {
			val = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].ob_db[ix].ob_dbValue;
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}
#endif //0
/*
 *Function Name:Qc98xxXpaBiasLvlGet
 *Parameters: returned string value for get value
 *Description: Get xpaBiasLvl value from field of eeprom struct u_int8_t
 *Returns: zero
 */
int Qc98xxXpaBiasLvlGet(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].xpaBiasLvl;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].xpaBiasLvl;
	}
    return value;
}
#if 0

/*
 *Function Name:Qc98xxTxFrameToDataStartGet
 *Parameters: returned string value for get value
 *Description: Get txFrameToDataStart value from field of eeprom struct u_int8_t
 *Returns: zero
 */
int Qc98xxTxFrameToDataStartGet(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txFrameToDataStart;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txFrameToDataStart;
	}
    return value;
}

/*
 *Function Name:Qc98xxTxFrameToPaOnGet
 *Parameters: returned string value for get value
 *Description: Get txFrameToPaOn value from field of eeprom struct u_int8_t
 *Returns: zero
 */
int Qc98xxTxFrameToPaOnGet(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txFrameToPaOn;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txFrameToPaOn;
	}
    return value;
}

/*
 *Function Name:Qc98xxTxClipGet
 *Parameters: returned string value for get value
 *Description: Get txClip value from field of eeprom struct u_int8_t (4 bits tx_clip)
 *Returns: zero
 */
int Qc98xxTxClipGet(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txClip & 0x0f;		// which 4 bits are for tx_clip???
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txClip & 0x0f;
	}
    return value;
}
/*
 *Function Name:Qc98xxDacScaleCckGet
 *Parameters: returned string value for get value
 *Description: Get txClip(dac_scale_cck) value from field of eeprom struct u_int8_t (4 bits tx_clip)
 *Returns: zero
 */
int Qc98xxDacScaleCckGet(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = (Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txClip & 0xf0) >> 4;		// which 4 bits are for dac_scale_cck???
	} else {
		value = (Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txClip & 0xf0) >> 4;
	}
    return value;
}
#endif //0
/*
 *Function Name:Qc98xxAntennaGainGet
 *Parameters: returned string value for get value
 *Description: Get antennaGain value from field of eeprom struct int8_t
 *Returns: zero
 */
int Qc98xxAntennaGainGet(int iBand)
{
	if (iBand==band_BG)
	{
		return (Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].antennaGainCh);
	}
	else
	{
		return (Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].antennaGainCh);
	}
}

#if 0
/*
 *Function Name:Qc98xxAdcDesiredSizeGet
 *Parameters: returned string value for get value
 *Description: Get adcDesiredSize value from field of eeprom struct int8_t
 *Returns: zero
 */
int Qc98xxAdcDesiredSizeGet(int iBand)
{
	A_INT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].adcDesiredSize;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].adcDesiredSize;
	}
    return value;
}

/*
 *Function Name:Qc98xxSwitchSettlingGet
 *Parameters: returned string value for get value
 *Description: Get switchSettling value from field of eeprom struct u_int8_t
 *Returns: zero
 */
int Qc98xxSwitchSettlingGet(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].switchSettling;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].switchSettling;
	}
    return value;
}

/*
 *Function Name:Qc98xxSwitchSettlingGet
 *Parameters: returned string value for get value
 *Description: Get switchSettling value from field of eeprom struct u_int8_t
 *Returns: zero
 */
int Qc98xxSwitchSettlingHt40Get(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		//value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].swSettleHt40;
	} else {
		//value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].swSettleHt40;
	}
    return value;
}

/*
 *Function Name:Qc98xxTxEndToXpaOffGet
 *Parameters: returned string value for get value
 *Description: Get txEndToXpaOff value from field of eeprom struct u_int8_t
 *Returns: zero
 */
int Qc98xxTxEndToXpaOffGet(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txEndToXpaOff;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txEndToXpaOff;
	}
    return value;
}
/*
 *Function Name:Qc98xxTxEndToRxOnGet
 *Parameters: returned string value for get value
 *Description: Get txEndToRxOn value from field of eeprom struct u_int8_t
 *Returns: zero
 */
int Qc98xxTxEndToRxOnGet(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txEndToRxOn;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txEndToRxOn;
	}
    return value;
}
/*
 *Function Name:Qc98xxTxFrameToXpaOnGet
 *Parameters: returned string value for get value
 *Description: Get txFrameToXpaOn value from field of eeprom struct u_int8_t
 *Returns: zero
 */
int Qc98xxTxFrameToXpaOnGet(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].txFrameToXpaOn;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].txFrameToXpaOn;
	}
    return value;
}
/*
 *Function Name:Qc98xxThresh62Get
 *Parameters: returned string value for get value
 *Description: Get thresh62 value from field of eeprom struct u_int8_t
 *Returns: zero
 */
int Qc98xxThresh62Get(int iBand)
{
	A_UINT8  value;
	if (iBand==band_BG) {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_2G_INDX].thresh62;
	} else {
		value = Qc98xxEepromStructGet()->biModalHeader[BIMODAL_5G_INDX].thresh62;
	}
    return value;
}
#endif //0

A_INT32 Qc98xxThermAdcScaledGainGet()
{
	A_INT16  value;
	value = Qc98xxEepromStructGet()->chipCalData.thermAdcScaledGain;
    return value;
}

A_INT32 Qc98xxThermAdcOffsetGet()
{
	A_INT8  value;
	value = Qc98xxEepromStructGet()->chipCalData.thermAdcOffset;
    return value;
}

/*
 *Function Name:Qc98xxCalFreqPierGet
 *Parameters: returned string value for get value
 *Description: Get calFreqPier flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxCalFreqPierGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iMaxPier;
	int val;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_CAL_PIERS : WHAL_NUM_11A_CAL_PIERS;

    if (ix<0 || ix>=iMaxPier)
    {
		for (i=0; i<iMaxPier; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->calFreqPier2G[i];
			} else {
				val = Qc98xxEepromStructGet()->calFreqPier5G[i];
			}
			value[i] = (val == WHAL_BCHAN_UNUSED) ? -1 : setFBIN2FREQ(val, iBand);
		}
		*num = iMaxPier;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->calFreqPier2G[ix];
		} else {
			val = Qc98xxEepromStructGet()->calFreqPier5G[ix];
		}
		value[0] = (val == WHAL_BCHAN_UNUSED) ? -1 : setFBIN2FREQ(val, iBand);
		*num = 1;
	}
	return VALUE_OK;
}

A_INT32 Qc98xxCalPointTxGainIdxGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iStart, iEnd;
	int j, jStart, jEnd;
	int k, kStart, kEnd;
	int iMaxPier, iv=0;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_CAL_PIERS : WHAL_NUM_11A_CAL_PIERS;
	*num = 0;
	if (ix<0 || ix>=iMaxPier)
	{
		iStart = 0;
		iEnd = iMaxPier;
	}
	else
	{
		iStart = ix;
		iEnd = ix + 1;
	}

	if (iy<0 || iy>=WHAL_NUM_CHAINS)
	{
		jStart = 0;
		jEnd = WHAL_NUM_CHAINS;
	}
	else
	{
		jStart = iy;
		jEnd = iy + 1;
	}
	if (iz<0 || iz>=WHAL_NUM_CAL_GAINS)
	{
		kStart = 0;
		kEnd = WHAL_NUM_CAL_GAINS;
	}
	else
	{
		kStart = iz;
		kEnd = iz + 1;
	}
	for (i=iStart; i<iEnd; i++)
    {
		for (j=jStart; j<jEnd; j++)
		{
			for (k=kStart; k<kEnd; k++)
			{
				if (iBand==band_BG)
				{
					value[iv++] =(int) Qc98xxEepromStructGet()->calPierData2G[i].calPerPoint[j].txgainIdx[k];
				}
				else
				{
					value[iv++] =(int) Qc98xxEepromStructGet()->calPierData5G[i].calPerPoint[j].txgainIdx[k];
				}
				(*num)++;
			}
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxCalPointDacGainGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iStart, iEnd;
	int j, jStart, jEnd;
	int iMaxPier, iv=0;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_CAL_PIERS : WHAL_NUM_11A_CAL_PIERS;
	*num = 0;
	if (ix<0 || ix>=iMaxPier)
	{
		iStart = 0;
		iEnd = iMaxPier;
	}
	else
	{
		iStart = ix;
		iEnd = ix + 1;
	}

	if (iy<0 || iy>=WHAL_NUM_CAL_GAINS)
	{
		jStart = 0;
		jEnd = WHAL_NUM_CAL_GAINS;
	}
	else
	{
		jStart = iy;
		jEnd = iy + 1;
	}
	for (i=iStart; i<iEnd; i++)
    {
		for (j=jStart; j<jEnd; j++)
		{
			if (iBand==band_BG)
			{
				value[iv++] =(int) (Qc98xxEepromStructGet()->calPierData2G[i].dacGain[j]);
			}
			else
			{
				value[iv++] =(int) (Qc98xxEepromStructGet()->calPierData5G[i].dacGain[j]);
			}
			(*num)++;
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxCalPointPowerGet(double *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iStart, iEnd;
	int j, jStart, jEnd;
	int k, kStart, kEnd;
	int iMaxPier, iv=0;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_CAL_PIERS : WHAL_NUM_11A_CAL_PIERS;
	*num = 0;
	if (ix<0 || ix>=iMaxPier)
	{
		iStart = 0;
		iEnd = iMaxPier;
	}
	else
	{
		iStart = ix;
		iEnd = ix + 1;
	}

	if (iy<0 || iy>=WHAL_NUM_CHAINS)
	{
		jStart = 0;
		jEnd = WHAL_NUM_CHAINS;
	}
	else
	{
		jStart = iy;
		jEnd = iy + 1;
	}
	if (iz<0 || iz>=WHAL_NUM_CAL_GAINS)
	{
		kStart = 0;
		kEnd = WHAL_NUM_CAL_GAINS;
	}
	else
	{
		kStart = iz;
		kEnd = iz + 1;
	}
	for (i=iStart; i<iEnd; i++)
    {
		for (j=jStart; j<jEnd; j++)
		{
			for (k=kStart; k<kEnd; k++)
			{
				if (iBand==band_BG)
				{
					value[iv++] =(int)(Qc98xxEepromStructGet()->calPierData2G[i].calPerPoint[j].power_t8[k] / 8);
				}
				else
				{
					value[iv++] =(int)(Qc98xxEepromStructGet()->calPierData5G[i].calPerPoint[j].power_t8[k] / 8);
				}
				(*num)++;
			}
		}
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxCalPierDataTempMeasGet
 *Parameters: returned string value for get value
 *Description: Get calPierData.tempMeas flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxCalPierDataTempMeasGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iMaxPier, iv=0;

    iMaxPier= (iBand==band_BG) ? WHAL_NUM_11G_CAL_PIERS : WHAL_NUM_11A_CAL_PIERS;

    if (ix<0 || ix>=iMaxPier)
    {
		for (i=0; i<iMaxPier; i++)
        {
			if (iBand==band_BG)
			{
				value[iv++] = Qc98xxEepromStructGet()->calPierData2G[i].thermCalVal;
			}
			else
			{
				value[iv++] = Qc98xxEepromStructGet()->calPierData5G[i].thermCalVal;
			}
		}
		*num = iMaxPier;
	}
    else
    {
		if (iBand==band_BG) {
			value[0] = Qc98xxEepromStructGet()->calPierData2G[ix].thermCalVal;
		} else {
			value[0] = Qc98xxEepromStructGet()->calPierData5G[ix].thermCalVal;
		}
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxCalPierDataVoltMeasGet
 *Parameters: returned string value for get value
 *Description: Get calPierData.voltMeas flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxCalPierDataVoltMeasGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iMaxPier, iv=0;

    iMaxPier= (iBand==band_BG) ? WHAL_NUM_11G_CAL_PIERS : WHAL_NUM_11A_CAL_PIERS;

    if (ix<0 || ix>=iMaxPier)
    {
		for (i=0; i<iMaxPier; i++)
        {
			if (iBand==band_BG)
			{
				value[iv++] = Qc98xxEepromStructGet()->calPierData2G[i].voltCalVal;
			}
			else
			{
				value[iv++] = Qc98xxEepromStructGet()->calPierData5G[i].voltCalVal;
			}
		}
		*num = iMaxPier;
	}
    else
    {
		if (iBand==band_BG) {
			value[0] = Qc98xxEepromStructGet()->calPierData2G[ix].voltCalVal;
		} else {
			value[0] = Qc98xxEepromStructGet()->calPierData5G[ix].voltCalVal;
		}
		*num = 1;
	}
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxCalPierDataRxNoisefloorCalGet
 *Parameters: returned string value for get value
 *Description: Get calPierData.rxNoisefloorCal flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxCalPierDataRxNoisefloorCalGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
    return VALUE_OK;
}
/*
 *Function Name:Qc98xxCalPierDataRxNoisefloorPowerGet
 *Parameters: returned string value for get value
 *Description: Get calPierData.rxNoisefloorPower flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxCalPierDataRxNoisefloorPowerGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
    return VALUE_OK;
}

/*
 *Function Name:Qc98xxCalPierDataRxTempMeasGet
 *Parameters: returned string value for get value
 *Description: Get calPierData.rxTempMeas flag in field of eeprom struct in QC98XX_MODAL_EEP_HEADER (u_int8_t)
 *Returns: zero
 */
A_INT32 Qc98xxCalPierDataRxTempMeasGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
    return VALUE_OK;
}

A_INT32 Qc98xxCalFreqTGTCckGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iMaxPier;
	int val;
	iMaxPier=WHAL_NUM_11B_TARGET_POWER_CHANS;
	if (ix<0 || ix>=iMaxPier)
    {
		for (i=0; i<iMaxPier; i++)
        {
			val = Qc98xxEepromStructGet()->targetFreqbinCck[i];
			value[i] = setFBIN2FREQ(val, iBand);
		}
		*num = iMaxPier;
	}
    else
    {
		val = Qc98xxEepromStructGet()->targetFreqbinCck[ix];
		value[0] = setFBIN2FREQ(val, iBand);
		*num = 1;
	}
	return VALUE_OK;
}

A_INT32 Qc98xxCalFreqTGTLegacyOFDMGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iMaxPier;
	int val;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11A_20_TARGET_POWER_CHANS;

    if (ix<0 || ix>=iMaxPier)
    {
		for (i=0; i<iMaxPier; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->targetFreqbin2G[i];
			} else {
				val = Qc98xxEepromStructGet()->targetFreqbin5G[i];
			}
			value[i] = setFBIN2FREQ(val, iBand);
		}
		*num = iMaxPier;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->targetFreqbin2G[ix];
		} else {
			val = Qc98xxEepromStructGet()->targetFreqbin5G[ix];
		}
		value[0] = setFBIN2FREQ(val, iBand);
		*num = 1;
	}
	return VALUE_OK;
}
A_INT32 Qc98xxCalFreqTGTHT20Get(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iMaxPier;
	int val;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11A_20_TARGET_POWER_CHANS;

    if (ix<0 || ix>=iMaxPier)
    {
		for (i=0; i<iMaxPier; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->targetFreqbin2GVHT20[i];
			} else {
				val = Qc98xxEepromStructGet()->targetFreqbin5GVHT20[i];
			}
			value[i] = setFBIN2FREQ(val, iBand);
		}
		*num = iMaxPier;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->targetFreqbin2GVHT20[ix];
		} else {
			val = Qc98xxEepromStructGet()->targetFreqbin5GVHT20[ix];
		}
		value[0] = setFBIN2FREQ(val, iBand);
		*num = 1;
	}
	return VALUE_OK;
}

A_INT32 Qc98xxCalFreqTGTHT40Get(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iMaxPier;
	int val;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_40_TARGET_POWER_CHANS : WHAL_NUM_11A_40_TARGET_POWER_CHANS;

    if (ix<0 || ix>=iMaxPier)
    {
		for (i=0; i<iMaxPier; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->targetFreqbin2GVHT40[i];
			} else {
				val = Qc98xxEepromStructGet()->targetFreqbin5GVHT40[i];
			}
			value[i] = setFBIN2FREQ(val, iBand);
		}
		*num = iMaxPier;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->targetFreqbin2GVHT40[ix];
		} else {
			val = Qc98xxEepromStructGet()->targetFreqbin5GVHT40[ix];
		}
		value[0] = setFBIN2FREQ(val, iBand);
		*num = 1;
	}
	return VALUE_OK;
}

A_INT32 Qc98xxCalFreqTGTHT80Get(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iMaxPier;
	int val;

	if (iBand==band_BG)
	{
		return ERR_VALUE_BAD;
	}
    iMaxPier = WHAL_NUM_11A_80_TARGET_POWER_CHANS;

    if (ix<0 || ix>=iMaxPier)
    {
		for (i=0; i<iMaxPier; i++)
        {
			val = Qc98xxEepromStructGet()->targetFreqbin5GVHT80[i];
			value[i] = setFBIN2FREQ(val, iBand);
		}
		*num = iMaxPier;
	}
    else
    {
		val = Qc98xxEepromStructGet()->targetFreqbin5GVHT80[ix];
		value[0] = setFBIN2FREQ(val, iBand);
		*num = 1;
	}
	return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrCCKGet(double *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, j, iv=0, iMaxPier, jMaxRate=WHAL_NUM_LEGACY_TARGET_POWER_RATES;
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
					value[iv++] = ((double)Qc98xxEepromStructGet()->targetPowerCck[i].tPow2x[j])/2.0;
				}
			}
			*num = jMaxRate*iMaxPier;
		}
        else
        { // get all j for ix chain
			for (j=0; j<jMaxRate; j++)
            {
				value[iv++] = ((double)Qc98xxEepromStructGet()->targetPowerCck[ix].tPow2x[j])/2.0;
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
				value[iv++] = ((double)Qc98xxEepromStructGet()->targetPowerCck[i].tPow2x[iy])/2.0;
				*num = iMaxPier;
			}
		}
        else
        {
			value[0] = ((double)Qc98xxEepromStructGet()->targetPowerCck[ix].tPow2x[iy])/2.0;
			*num = 1;
		}
	}
	return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrLegacyOFDMGet(double *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, j, iv=0, iMaxPier, jMaxRate=WHAL_NUM_LEGACY_TARGET_POWER_RATES;

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
						value[iv++] = ((double)Qc98xxEepromStructGet()->targetPower2G[i].tPow2x[j])/2.0;
					} else {
						value[iv++] = ((double)Qc98xxEepromStructGet()->targetPower5G[i].tPow2x[j])/2.0;
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
					value[iv++] = ((double)Qc98xxEepromStructGet()->targetPower2G[ix].tPow2x[j])/2.0;
				} else {
					value[iv++] = ((double)Qc98xxEepromStructGet()->targetPower5G[ix].tPow2x[j])/8.0;
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
					value[iv++] = ((double)Qc98xxEepromStructGet()->targetPower2G[i].tPow2x[iy])/2.0;
				} else {
					value[iv++] = ((double)Qc98xxEepromStructGet()->targetPower5G[i].tPow2x[iy])/2.0;
				}
				*num = iMaxPier;
			}
		}
        else
        {
			if (iBand==band_BG) {
				value[0] =((double) Qc98xxEepromStructGet()->targetPower2G[ix].tPow2x[iy])/2.0;
			} else {
				value[0] = ((double)Qc98xxEepromStructGet()->targetPower5G[ix].tPow2x[iy])/2.0;
			}
			*num = 1;
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrHT20Get(double *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, j, iv=0, iMaxPier, jMaxRate;

    iMaxPier = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_CHANS : WHAL_NUM_11A_20_TARGET_POWER_CHANS;
	jMaxRate = (iBand==band_BG) ? WHAL_NUM_11G_20_TARGET_POWER_RATES : WHAL_NUM_11A_20_TARGET_POWER_RATES;

	if (ix >= iMaxPier || iy >= VHT_TARGET_RATE_LAST)
	{
		return ERR_RETURN;
	}
	if (iy >= jMaxRate)
	{
		printf("Please ignore the above \"Array index\" error\n");
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
						value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xBase,
															Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xDelta, j, FALSE,
                                                            i, CAL_TARGET_POWER_VHT20_INDEX, TRUE))/2.0;
					} else {
						value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xBase,
															Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xDelta, j, FALSE,
                                                            i, CAL_TARGET_POWER_VHT20_INDEX, FALSE))/2.0;
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
					value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT20[ix].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower2GVHT20[ix].tPow2xDelta, j, FALSE,
                                                        ix, CAL_TARGET_POWER_VHT20_INDEX, TRUE))/2.0;
				} else {
					value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT20[ix].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower5GVHT20[ix].tPow2xDelta, j, FALSE,
                                                        ix, CAL_TARGET_POWER_VHT20_INDEX, FALSE))/2.0;
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
					value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower2GVHT20[i].tPow2xDelta, iy, FALSE,
                                                        i, CAL_TARGET_POWER_VHT20_INDEX, TRUE))/2.0;
				} else {
					value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower5GVHT20[i].tPow2xDelta, iy, FALSE,
                                                        i, CAL_TARGET_POWER_VHT20_INDEX, FALSE))/2.0;
				}
				*num = iMaxPier;
			}
		}
        else
        {
			if (iBand==band_BG) {
				value[0] =((double) GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT20[ix].tPow2xBase,
												Qc98xxEepromStructGet()->targetPower2GVHT20[ix].tPow2xDelta, iy, FALSE,
                                                ix, CAL_TARGET_POWER_VHT20_INDEX, TRUE))/2.0;
			} else {
				value[0] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT20[ix].tPow2xBase,
												Qc98xxEepromStructGet()->targetPower5GVHT20[ix].tPow2xDelta, iy, FALSE,
                                                ix, CAL_TARGET_POWER_VHT20_INDEX, FALSE))/2.0;
			}
			*num = 1;
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrHT40Get(double *value, int ix, int iy, int iz, int *num, int iBand)
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
		printf("Please ignore the above \"Array index\" error\n");
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
						value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xBase,
															Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xDelta, j, FALSE,
                                                            i, CAL_TARGET_POWER_VHT40_INDEX, TRUE))/2.0;
					} else {
						value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xBase,
															Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xDelta, j, FALSE,
                                                            i, CAL_TARGET_POWER_VHT40_INDEX, FALSE))/2.0;
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
					value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT40[ix].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower2GVHT40[ix].tPow2xDelta, j, FALSE,
                                                        ix, CAL_TARGET_POWER_VHT40_INDEX, TRUE))/2.0;
				} else {
					value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT40[ix].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower5GVHT40[ix].tPow2xDelta, j, FALSE,
                                                        ix, CAL_TARGET_POWER_VHT40_INDEX, FALSE))/2.0;
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
					value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower2GVHT40[i].tPow2xDelta, iy, FALSE,
                                                        i, CAL_TARGET_POWER_VHT40_INDEX, TRUE))/2.0;
				} else {
					value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower5GVHT40[i].tPow2xDelta, iy, FALSE,
                                                        i, CAL_TARGET_POWER_VHT40_INDEX, FALSE))/2.0;
				}
				*num = iMaxPier;
			}
		}
        else
        {
			if (iBand==band_BG) {
				value[0] =((double) GetHttPow2x(Qc98xxEepromStructGet()->targetPower2GVHT40[ix].tPow2xBase,
												Qc98xxEepromStructGet()->targetPower2GVHT40[ix].tPow2xDelta, iy, FALSE,
                                                ix, CAL_TARGET_POWER_VHT40_INDEX, TRUE))/2.0;
			} else {
				value[0] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT40[ix].tPow2xBase,
												Qc98xxEepromStructGet()->targetPower5GVHT40[ix].tPow2xDelta, iy, FALSE,
                                                ix, CAL_TARGET_POWER_VHT40_INDEX, FALSE))/2.0;
			}
			*num = 1;
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxCalTGTPwrHT80Get(double *value, int ix, int iy, int iz, int *num, int iBand)
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
		printf("Please ignore the above \"Array index\" error\n");
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
					value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xBase,
														Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xDelta, j, FALSE,
                                                        i, CAL_TARGET_POWER_VHT80_INDEX, FALSE))/2.0;
				}
			}
			*num = jMaxRate*iMaxPier;
		}
        else
        { // get all j for ix chain
			for (j=0; j<jMaxRate; j++)
            {
				value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT80[ix].tPow2xBase,
													Qc98xxEepromStructGet()->targetPower5GVHT80[ix].tPow2xDelta, j, FALSE,
                                                    ix, CAL_TARGET_POWER_VHT80_INDEX, FALSE))/2.0;
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
				value[iv++] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xBase,
													Qc98xxEepromStructGet()->targetPower5GVHT80[i].tPow2xDelta, iy, FALSE,
                                                    i, CAL_TARGET_POWER_VHT80_INDEX, FALSE))/2.0;
				*num = iMaxPier;
			}
		}
        else
        {
			value[0] = ((double)GetHttPow2x(Qc98xxEepromStructGet()->targetPower5GVHT80[ix].tPow2xBase,
											Qc98xxEepromStructGet()->targetPower5GVHT80[ix].tPow2xDelta, iy, FALSE,
                                            ix, CAL_TARGET_POWER_VHT80_INDEX, FALSE))/2.0;
			*num = 1;
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxCtlIndexGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iMaxCtl;
	int val;

    iMaxCtl = (iBand==band_BG) ? WHAL_NUM_CTLS_2G : WHAL_NUM_CTLS_5G;
	if (ix<0 || ix>(iMaxCtl-1))
    {
		for (i=0; i<iMaxCtl; i++)
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->ctlIndex2G[i];
			} else {
				val = Qc98xxEepromStructGet()->ctlIndex5G[i];
			}
			value[i] = val;
		}
		*num = iMaxCtl;
	}
    else
    {
		if (iBand==band_BG) {
			val = Qc98xxEepromStructGet()->ctlIndex2G[ix];
		} else {
			val = Qc98xxEepromStructGet()->ctlIndex5G[ix];
		}
		value[0] = val;
		*num = 1;
	}
    return VALUE_OK;
}

A_INT32 Qc98xxCtlFreqGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, j, iv=0, iMaxCtl, jMaxEdge;
	int val;
	if (iBand==band_BG)
    {
		jMaxEdge=WHAL_NUM_BAND_EDGES_2G;
		iMaxCtl=WHAL_NUM_CTLS_2G;
	}
    else
    {
		jMaxEdge=WHAL_NUM_BAND_EDGES_5G;
		iMaxCtl=WHAL_NUM_CTLS_5G;
	}
	if (iy<0 || iy>=jMaxEdge)
    {
		if (ix<0 || ix>=iMaxCtl)
        {
			// get all i, all j
			for (i=0; i<iMaxCtl; i++)
            {
				for (j=0; j<jMaxEdge; j++)
                {
					if (iBand==band_BG) {
						val = Qc98xxEepromStructGet()->ctlFreqbin2G[i][j];
					} else {
						val = Qc98xxEepromStructGet()->ctlFreqbin5G[i][j];
					}
					value[iv++]=setFBIN2FREQ(val, iBand);
				}
			}
			*num = jMaxEdge*iMaxCtl;
		}
        else
        { // get all j for ix chain
			for (j=0; j<jMaxEdge; j++)
            {
				if (iBand==band_BG)
                {
					val = Qc98xxEepromStructGet()->ctlFreqbin2G[ix][j];
				} else {
					val = Qc98xxEepromStructGet()->ctlFreqbin5G[ix][j];
				}
				value[iv++]=setFBIN2FREQ(val, iBand);
			}
			*num = jMaxEdge;
		}
	}
    else
    {
		if (ix<0 || ix>=iMaxCtl)
        {
			for (i=0; i<iMaxCtl; i++)
            {
				if (iBand==band_BG) {
					val = Qc98xxEepromStructGet()->ctlFreqbin2G[i][iy];
				} else {
					val = Qc98xxEepromStructGet()->ctlFreqbin5G[i][iy];
				}
				value[iv++]=setFBIN2FREQ(val, iBand);
				*num = iMaxCtl;
			}
		}
        else
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->ctlFreqbin2G[ix][iy];
			} else {
				val = Qc98xxEepromStructGet()->ctlFreqbin5G[ix][iy];
			}
			value[0]=setFBIN2FREQ(val, iBand);
			*num = 1;
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxCtlPowerGet(double *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, j, iv=0, iMaxCtl, jMaxEdge;
	int val;

    if (iBand==band_BG)
    {
		jMaxEdge=WHAL_NUM_BAND_EDGES_2G;
		iMaxCtl=WHAL_NUM_CTLS_2G;
	}
    else
    {
		jMaxEdge=WHAL_NUM_BAND_EDGES_5G;
		iMaxCtl=WHAL_NUM_CTLS_5G;
	}
	if (iy<0 || iy>=jMaxEdge)
    {
		if (ix<0 || ix>=iMaxCtl)
        {
			// get all i, all j
			for (i=0; i<iMaxCtl; i++)
            {
				for (j=0; j<jMaxEdge; j++)
                {
					if (iBand==band_BG) {
						val = Qc98xxEepromStructGet()->ctlData2G[i].ctl_edges[j].u.tPower;
					} else {
						val = Qc98xxEepromStructGet()->ctlData5G[i].ctl_edges[j].u.tPower;
					}
					value[iv++]=val/2.0;
				}
			}
			*num = jMaxEdge*iMaxCtl;
		}
        else
        { // get all j for ix chain
			for (j=0; j<jMaxEdge; j++)
            {
				if (iBand==band_BG) {
					val = Qc98xxEepromStructGet()->ctlData2G[ix].ctl_edges[j].u.tPower;
				} else {
					val = Qc98xxEepromStructGet()->ctlData5G[ix].ctl_edges[j].u.tPower;
				}
				value[iv++]=val/2.0;
			}
			*num = jMaxEdge;
		}
	}
    else
    {
		if (ix<0 || ix>=iMaxCtl)
        {
			for (i=0; i<iMaxCtl; i++)
            {
				if (iBand==band_BG) {
					val = Qc98xxEepromStructGet()->ctlData2G[i].ctl_edges[iy].u.tPower;
				} else {
					val = Qc98xxEepromStructGet()->ctlData5G[i].ctl_edges[iy].u.tPower;
				}
				value[iv++]=val/2.0;
				*num = iMaxCtl;
			}
		}
        else
        {
			if (iBand==band_BG) {
				val = Qc98xxEepromStructGet()->ctlData2G[ix].ctl_edges[iy].u.tPower;
			} else {
				val = Qc98xxEepromStructGet()->ctlData5G[ix].ctl_edges[iy].u.tPower;
			}
			value[0]=val/2.0;
			*num = 1;
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxCtlFlagGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, j, iv=0, iMaxCtl, jMaxEdge;

    if (iBand==band_BG)
    {
		jMaxEdge=WHAL_NUM_BAND_EDGES_2G;
		iMaxCtl=WHAL_NUM_CTLS_2G;
	}
    else
    {
		jMaxEdge=WHAL_NUM_BAND_EDGES_5G;
		iMaxCtl=WHAL_NUM_CTLS_5G;
	}
	if (iy<0 || iy>=jMaxEdge)
    {
		if (ix<0 || ix>=iMaxCtl)
        {
			// get all i, all j
			for (i=0; i<iMaxCtl; i++)
            {
				for (j=0; j<jMaxEdge; j++)
                {
					if (iBand==band_BG) {
						value[iv++] = Qc98xxEepromStructGet()->ctlData2G[i].ctl_edges[j].u.flag;
					} else {
						value[iv++] = Qc98xxEepromStructGet()->ctlData5G[i].ctl_edges[j].u.flag;
					}
				}
			}
			*num = jMaxEdge*iMaxCtl;
		}
        else
        { // get all j for ix chain
			for (j=0; j<jMaxEdge; j++)
            {
				if (iBand==band_BG) {
					value[iv++] = Qc98xxEepromStructGet()->ctlData2G[ix].ctl_edges[j].u.flag;
				} else {
					value[iv++] = Qc98xxEepromStructGet()->ctlData5G[ix].ctl_edges[j].u.flag;
				}
			}
			*num = jMaxEdge;
		}
	}
    else
    {
		if (ix<0 || ix>=iMaxCtl)
        {
			for (i=0; i<iMaxCtl; i++)
            {
				if (iBand==band_BG) {
					value[iv++] = Qc98xxEepromStructGet()->ctlData2G[i].ctl_edges[iy].u.flag;
				} else {
					value[iv++] = Qc98xxEepromStructGet()->ctlData5G[i].ctl_edges[iy].u.flag;
				}
				*num = iMaxCtl;
			}
		}
        else
        {
			if (iBand==band_BG) {
				value[0] = Qc98xxEepromStructGet()->ctlData2G[ix].ctl_edges[iy].u.flag;
			} else {
				value[0] = Qc98xxEepromStructGet()->ctlData5G[ix].ctl_edges[iy].u.flag;
			}
			*num = 1;
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxAlphaThermTableGet(int *value, int ix, int iy, int iz, int *num, int iBand)
{
	int i, iStart, iEnd;
	int j, jStart, jEnd;
	int k, kStart, kEnd;
	int iv=0;

	*num = 0;
	if (ix<0 || ix>=WHAL_NUM_CHAINS)
	{
		iStart = 0;
		iEnd = WHAL_NUM_CHAINS;
	}
	else
	{
		iStart = ix;
		iEnd = ix + 1;
	}

	jEnd = (iBand==band_BG) ? QC98XX_NUM_ALPHATHERM_CHANS_2G : QC98XX_NUM_ALPHATHERM_CHANS_5G;
	if (iy<0 || iy>=jEnd)
	{
		jStart = 0;
	}
	else
	{
		jStart = iy;
		jEnd = iy + 1;
	}
	if (iz<0 || iz>=QC98XX_NUM_ALPHATHERM_TEMPS)
	{
		kStart = 0;
		kEnd = QC98XX_NUM_ALPHATHERM_TEMPS;
	}
	else
	{
		kStart = iz;
		kEnd = iz + 1;
	}
	for (i=iStart; i<iEnd; i++)
    {
		for (j=jStart; j<jEnd; j++)
		{
			for (k=kStart; k<kEnd; k++)
			{
				if (iBand==band_BG)
				{
					value[iv++] =(int) Qc98xxEepromStructGet()->tempComp2G.alphaThermTbl[i][j][k];
				}
				else
				{
					value[iv++] =(int) Qc98xxEepromStructGet()->tempComp5G.alphaThermTbl[i][j][k];
				}
				(*num)++;
			}
		}
	}
    return VALUE_OK;
}

A_INT32 Qc98xxConfigAddrGet(int *value, int ix, int *num)
{
	int i;

	if (ix<0 || ix>=QC98XX_CONFIG_ENTRIES)
    {
		for (i=0; i<QC98XX_CONFIG_ENTRIES; i++)
        {
			value[i] = Qc98xxEepromStructGet()->configAddr[i];
		}
		*num = QC98XX_CONFIG_ENTRIES;
	}
    else
    {
		value[0] = Qc98xxEepromStructGet()->configAddr[ix];
		*num = 1;
	}
    return VALUE_OK;
}

#if 0
int Qc98xxEepromPaPredistortionGet(void)
{
	return ((Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.featureEnable & WHAL_FEATUREENABLE_PAPRD_MASK)
			>> WHAL_FEATUREENABLE_PAPRD_SHIFT);
}
#endif //0

int Qc98xxEepromCalibrationValid(void)
{
    return 0;
}

int Qc98xxEepromLengthGet()
{
    return (Qc98xxEepromStructGet()->baseEepHeader.length);
}


