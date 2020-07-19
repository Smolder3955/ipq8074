
#ifndef __INCrateconstantsh
#define __INCrateconstantsh

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum {
    RATE_INDEX_6=0,
    RATE_INDEX_9,
    RATE_INDEX_12,
    RATE_INDEX_18,
    RATE_INDEX_24,
    RATE_INDEX_36,
    RATE_INDEX_48,
    RATE_INDEX_54,
    RATE_INDEX_1L,
    RATE_INDEX_2L,
    RATE_INDEX_2S,
    RATE_INDEX_5L,
    RATE_INDEX_5S,
    RATE_INDEX_11L,
    RATE_INDEX_11S,
    RATE_INDEX_HT20_MCS0=32,
    RATE_INDEX_HT20_MCS1,
    RATE_INDEX_HT20_MCS2,
    RATE_INDEX_HT20_MCS3,
    RATE_INDEX_HT20_MCS4,
    RATE_INDEX_HT20_MCS5,
    RATE_INDEX_HT20_MCS6,
    RATE_INDEX_HT20_MCS7,
    RATE_INDEX_HT20_MCS8,
    RATE_INDEX_HT20_MCS9,
    RATE_INDEX_HT20_MCS10,
    RATE_INDEX_HT20_MCS11,
    RATE_INDEX_HT20_MCS12,
    RATE_INDEX_HT20_MCS13,
    RATE_INDEX_HT20_MCS14,
    RATE_INDEX_HT20_MCS15,
    RATE_INDEX_HT20_MCS16,
    RATE_INDEX_HT20_MCS17,
    RATE_INDEX_HT20_MCS18,
    RATE_INDEX_HT20_MCS19,
    RATE_INDEX_HT20_MCS20,
    RATE_INDEX_HT20_MCS21,
    RATE_INDEX_HT20_MCS22,
    RATE_INDEX_HT20_MCS23,
    RATE_INDEX_HT40_MCS0=64,
    RATE_INDEX_HT40_MCS1,
    RATE_INDEX_HT40_MCS2,
    RATE_INDEX_HT40_MCS3,
    RATE_INDEX_HT40_MCS4,
    RATE_INDEX_HT40_MCS5,
    RATE_INDEX_HT40_MCS6,
    RATE_INDEX_HT40_MCS7,
    RATE_INDEX_HT40_MCS8,
    RATE_INDEX_HT40_MCS9,
    RATE_INDEX_HT40_MCS10,
    RATE_INDEX_HT40_MCS11,
    RATE_INDEX_HT40_MCS12,
    RATE_INDEX_HT40_MCS13,
    RATE_INDEX_HT40_MCS14,
    RATE_INDEX_HT40_MCS15,
    RATE_INDEX_HT40_MCS16,
    RATE_INDEX_HT40_MCS17,
    RATE_INDEX_HT40_MCS18,
    RATE_INDEX_HT40_MCS19,
    RATE_INDEX_HT40_MCS20,
    RATE_INDEX_HT40_MCS21,
    RATE_INDEX_HT40_MCS22,
    RATE_INDEX_HT40_MCS23,
}RATE_INDEX ;

enum
{
	RateAll=1000,
	RateLegacy,
	RateHt20,
	RateHt40,
	RateDvt,
};


//const A_UINT16 numRateCodes = sizeof(rateCodes)/sizeof(A_UCHAR);
#define numRateCodes 96
extern const A_UCHAR rateValues[numRateCodes];
extern const A_UCHAR rateCodes[numRateCodes];
extern char *rateStrAll[numRateCodes];

#define UNKNOWN_RATE_CODE 0xff
#define IS_1STREAM_RATE_INDEX(x) (((x) >= RATE_INDEX_HT20_MCS0 && (x) <= RATE_INDEX_HT20_MCS7) || ((x) >= RATE_INDEX_HT40_MCS0 && (x) <= RATE_INDEX_HT40_MCS7))
#define IS_2STREAM_RATE_INDEX(x) (((x) >= RATE_INDEX_HT20_MCS8 && (x) <= RATE_INDEX_HT20_MCS15) || ((x) >= RATE_INDEX_HT40_MCS8 && (x) <= RATE_INDEX_HT40_MCS15))
#define IS_3STREAM_RATE_INDEX(x) (((x) >= RATE_INDEX_HT20_MCS16 && (x) <= RATE_INDEX_HT20_MCS23) || ((x) >= RATE_INDEX_HT40_MCS16 && (x) <= RATE_INDEX_HT40_MCS23))
#define IS_HT40_RATE_INDEX(x)    ((x) >= RATE_INDEX_HT40_MCS0 && (x) < (RATE_INDEX_HT40_MCS0 + (8*4)))
#define IS_HT20_RATE_INDEX(x)    ((x) >= RATE_INDEX_HT20_MCS0 && (x) < (RATE_INDEX_HT20_MCS0 + (8*4)))
#define IS_LEGACY_RATE_INDEX(x)  ((x) >= RATE_INDEX_6 && (x) < RATE_INDEX_HT20_MCS0)
#define IS_11B_RATE_INDEX(x)     ((x) >= RATE_INDEX_1L && (x) <= 23)
#define IS_11G_RATE_INDEX(x)     ((x) >= RATE_INDEX_HT20_MCS0 && (x) <= RATE_INDEX_54)

extern A_UINT32 descRate2bin(A_UINT32 descRateCode);
extern A_UINT32 descRate2RateIndex(A_UINT32 descRateCode, A_BOOL ht40);
extern A_UINT32 rate2bin(A_UINT32 rateCode);
extern int RateCount(A_UINT32 rateMask, A_UINT32 rateMaskMcs20, A_UINT32 rateMaskMcs40, int *Rate);
extern int RateExpand(int *rate, int nrate);
extern void RateMaskGet (A_UINT32 *rateMask, A_UINT32 *rateMaskMcs20, A_UINT32 *rateMaskMcs40, int *Rate, int nrate);
extern void RateMask2UtfRateMask (A_UINT32 rateMask, A_UINT32 rateMaskMcs20, A_UINT32 rateMaskMcs40,
						   A_UINT32 *utfRateMask);
extern int UtfRateBit2RateIndx(A_UINT32 rateBit);
extern int RateIndx2UtfRateBit(int rateIndx);

extern int AdjustFreqBasedOnRateAndBandwidth(int frequency, int *rate, int bandwidth);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

