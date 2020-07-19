#ifndef __OL_IF_EEPROM_H__
#define __OL_IF_EEPROM_H__

#ifdef WIN32
#include <pshpack1.h>
#endif

#ifndef _A_TYPES_H_
typedef    u_int8_t     A_UINT8;
typedef    int8_t       A_INT8;
typedef    u_int16_t    A_UINT16;
typedef    int16_t      A_INT16;
typedef    u_int32_t    A_UINT32;
typedef    int32_t      A_INT32;
typedef    u_int64_t    A_UINT64;
typedef    bool         A_BOOL;
#define    FALSE        0
#define    TRUE         1
#endif

/*
 * TODO: Only AR98XX is supported currently
 */

#define NUM_STREAMS 3
#define NUM_2G_TGT_POWER_CHANS 3
#define NUM_5G_TGT_POWER_CHANS 6
#define NUM_2G_TGT_POWER_CCK_CHANS 2

#define OVERLAP_DATA_RATE_CCK 4
#define OVERLAP_DATA_RATE_LEGACY 4
#define OVERLAP_DATA_RATE_VHT 8

#define WHAL_NUM_CHAINS                         3
#define WHAL_NUM_STREAMS                        3   
#define WHAL_NUM_BI_MODAL                       2
#define WHAL_NUM_AC_MODAL                       5

#define WHAL_NUM_11B_TARGET_POWER_CHANS             2
#define WHAL_NUM_11G_LEG_TARGET_POWER_CHANS         3
#define WHAL_NUM_11G_20_TARGET_POWER_CHANS          3
#define WHAL_NUM_11G_40_TARGET_POWER_CHANS          3
#define WHAL_NUM_11A_LEG_TARGET_POWER_CHANS         6
#define WHAL_NUM_11A_20_TARGET_POWER_CHANS          6
#define WHAL_NUM_11A_40_TARGET_POWER_CHANS          6
#define WHAL_NUM_11A_80_TARGET_POWER_CHANS          6

#define WHAL_NUM_LEGACY_TARGET_POWER_RATES          4
#define WHAL_NUM_11B_TARGET_POWER_RATES             4
#define WHAL_NUM_VHT_TARGET_POWER_RATES             (5*WHAL_NUM_STREAMS+3)  //18 make sure this is not odd number
#define WHAL_NUM_HT_TARGET_POWER_RATES              WHAL_NUM_VHT_TARGET_POWER_RATES         
#define WHAL_NUM_11G_20_TARGET_POWER_RATES          (WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11G_40_TARGET_POWER_RATES          (WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11A_20_TARGET_POWER_RATES          (WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11A_40_TARGET_POWER_RATES          (WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11A_80_TARGET_POWER_RATES          (WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11G_20_TARGET_POWER_RATES_DIV2     (WHAL_NUM_VHT_TARGET_POWER_RATES/2)
#define WHAL_NUM_11G_40_TARGET_POWER_RATES_DIV2     (WHAL_NUM_VHT_TARGET_POWER_RATES/2)
#define WHAL_NUM_11A_20_TARGET_POWER_RATES_DIV2     (WHAL_NUM_VHT_TARGET_POWER_RATES/2)
#define WHAL_NUM_11A_40_TARGET_POWER_RATES_DIV2     (WHAL_NUM_VHT_TARGET_POWER_RATES/2)
#define WHAL_NUM_11A_80_TARGET_POWER_RATES_DIV2     (WHAL_NUM_VHT_TARGET_POWER_RATES/2)

/* Ensure this the MAX target power rates are accurate */
#define WHAL_NUM_MAX_TARGET_POWER_RATES             WHAL_NUM_VHT_TARGET_POWER_RATES

// Target power rates grouping
enum {
    WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20= 0,
    WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22,
    WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24,
    WHAL_VHT_TARGET_POWER_RATES_MCS5,
    WHAL_VHT_TARGET_POWER_RATES_MCS6,
    WHAL_VHT_TARGET_POWER_RATES_MCS7,
    WHAL_VHT_TARGET_POWER_RATES_MCS8,
    WHAL_VHT_TARGET_POWER_RATES_MCS9,
    WHAL_VHT_TARGET_POWER_RATES_MCS15,
    WHAL_VHT_TARGET_POWER_RATES_MCS16,
    WHAL_VHT_TARGET_POWER_RATES_MCS17,
    WHAL_VHT_TARGET_POWER_RATES_MCS18,
    WHAL_VHT_TARGET_POWER_RATES_MCS19,
    WHAL_VHT_TARGET_POWER_RATES_MCS25,
    WHAL_VHT_TARGET_POWER_RATES_MCS26,
    WHAL_VHT_TARGET_POWER_RATES_MCS27,
    WHAL_VHT_TARGET_POWER_RATES_MCS28,
    WHAL_VHT_TARGET_POWER_RATES_MCS29,
};

typedef struct qc98xxUsergtPower
{
    A_UINT32 validmask; /*bit 0 indicates valid 2G, bit 1 indicates valid 5G */    // 4B
/* We could extend for individual valid maks for cck, legacy2g etc.. */
    /* 1L_5L,5S,11L,11S */
    A_UINT8 cck_2G [OVERLAP_DATA_RATE_CCK * NUM_2G_TGT_POWER_CCK_CHANS];    /*  rates * number of channels */ //8B
    /* r6_24,r36,r48,r54 */
    A_UINT8 legacy_2G [OVERLAP_DATA_RATE_LEGACY * NUM_2G_TGT_POWER_CHANS]; /*  rates * number of channels */  //12B
    /* MCS0, MCS_1_2, MCS_3_4, MCS5, MCS6, MCS7, MCS8, MCS9 */
    A_UINT8 vht20_2G [ OVERLAP_DATA_RATE_VHT  * NUM_STREAMS * NUM_2G_TGT_POWER_CHANS]; /* mixed rates * number of streams * number of channels */ //72B
    /* MCS0, MCS_1_2, MCS_3_4, MCS5, MCS6, MCS7, MCS8, MCS9 */
    A_UINT8 vht40_2G [ OVERLAP_DATA_RATE_VHT * NUM_STREAMS * NUM_2G_TGT_POWER_CHANS]; /* mixed rates * number of streams * number of channels */ //72B

    /* r6_24,r36,r48,r54 */
    A_UINT8 legacy_5G [OVERLAP_DATA_RATE_LEGACY * NUM_5G_TGT_POWER_CHANS]; /*  rates * number of channels */           //24B
    /* MCS0, MCS_1_2, MCS_3_4, MCS5, MCS6, MCS7, MCS8, MCS9 */
    A_UINT8 vht20_5G [ OVERLAP_DATA_RATE_VHT * NUM_STREAMS * NUM_5G_TGT_POWER_CHANS]; /* mixed rates * number of streams * number of channels */ //144B
    A_UINT8 vht40_5G [ OVERLAP_DATA_RATE_VHT * NUM_STREAMS * NUM_5G_TGT_POWER_CHANS]; /* mixed rates * number of streams * number of channels */ //144B
    A_UINT8 vht80_5G [ OVERLAP_DATA_RATE_VHT * NUM_STREAMS * NUM_5G_TGT_POWER_CHANS]; /* mixed rates * number of streams * number of channels */ //144B

} __packed QC98XX_USER_TGT_POWER;

#define CAL_TARGET_POWER_VHT20_INDEX    0
#define CAL_TARGET_POWER_VHT40_INDEX    1
#define CAL_TARGET_POWER_VHT80_INDEX    2
#define QC98XX_EXT_TARGET_POWER_SIZE_2G  16
#define QC98XX_EXT_TARGET_POWER_SIZE_5G  44

typedef struct {
    A_UINT8  tPow2x[WHAL_NUM_LEGACY_TARGET_POWER_RATES];
} __packed CAL_TARGET_POWER_LEG;                       // 4B

typedef struct {
    A_UINT8  tPow2x[WHAL_NUM_11B_TARGET_POWER_RATES];
} __packed CAL_TARGET_POWER_11B;                       // 4B

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11G_20_TARGET_POWER_RATES_DIV2];
} __packed CAL_TARGET_POWER_11G_20;                    // 12B

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11G_40_TARGET_POWER_RATES_DIV2];
} __packed CAL_TARGET_POWER_11G_40;                    // 12B

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11A_20_TARGET_POWER_RATES_DIV2];
} __packed CAL_TARGET_POWER_11A_20;                    // 12B

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11A_40_TARGET_POWER_RATES_DIV2];
} __packed CAL_TARGET_POWER_11A_40;                    // 12B

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11A_80_TARGET_POWER_RATES_DIV2];
} __packed CAL_TARGET_POWER_11A_80;                    // 12B

typedef struct qc98xxEepromRateTbl
{
    A_UINT32                      validmask; // Bit 0 indicates valid 2G, Bit 1 indicates valid 5G // 4B
    A_UINT8                       extTPow2xDelta2G[QC98XX_EXT_TARGET_POWER_SIZE_2G];            // 16B
    CAL_TARGET_POWER_11B          targetPowerCck[WHAL_NUM_11B_TARGET_POWER_CHANS];              // 8B
    CAL_TARGET_POWER_LEG          targetPower2G[WHAL_NUM_11G_LEG_TARGET_POWER_CHANS];           // 12B
    CAL_TARGET_POWER_11G_20       targetPower2GVHT20[WHAL_NUM_11G_20_TARGET_POWER_CHANS];       // 36B
    CAL_TARGET_POWER_11G_40       targetPower2GVHT40[WHAL_NUM_11G_40_TARGET_POWER_CHANS];       // 36B
    A_UINT8                       extTPow2xDelta5G[QC98XX_EXT_TARGET_POWER_SIZE_5G];            // 44B
    CAL_TARGET_POWER_LEG          targetPower5G[WHAL_NUM_11A_LEG_TARGET_POWER_CHANS];           // 24B
    CAL_TARGET_POWER_11A_20       targetPower5GVHT20[WHAL_NUM_11A_20_TARGET_POWER_CHANS];       // 72B
    CAL_TARGET_POWER_11A_40       targetPower5GVHT40[WHAL_NUM_11A_40_TARGET_POWER_CHANS];       // 72B
    CAL_TARGET_POWER_11A_80       targetPower5GVHT80[WHAL_NUM_11A_80_TARGET_POWER_CHANS];       // 72B
} __packed QC98XX_EEPROM_RATETBL;

// The algorithm to set the target powers is as follow:
// Look for the smallest target power for all stream rates. -> stream
// Set tPow2xBase[stream] to the smallest
// Since MCS0, MCS10 and MCS20 share the same delta, the other base target powers should be adjusted accordingly
// Then set the other deltas accordingly
// Limitation: MCSs 1_2_11_12_21_22 share the same delta, and MCSs 3-4-13-14-23-24 share the same delta. 
//             Since the base target powers are set based on MCSs 0_10_20, the target powers for the above rates might be adjusted to
//             to the nearest posibble.

#define VHT_TARGET_RATE_0_10_20(x) (((x) == VHT_TARGET_RATE_0) || ((x) == VHT_TARGET_RATE_10) || ((x) == VHT_TARGET_RATE_20))
#define VHT_TARGET_RATE_1_2_11_12_21_22(x) (((x) == VHT_TARGET_RATE_1_2) || ((x) == VHT_TARGET_RATE_11_12) || ((x) == VHT_TARGET_RATE_21_22))
#define VHT_TARGET_RATE_3_4_13_14_23_24(x) (((x) == VHT_TARGET_RATE_3_4) || ((x) == VHT_TARGET_RATE_13_14) || ((x) == VHT_TARGET_RATE_23_24))
#define NOT_VHT_TARGET_RATE_LOW_GROUPS(x) (!VHT_TARGET_RATE_0_10_20(x) && !VHT_TARGET_RATE_1_2_11_12_21_22(x) && !VHT_TARGET_RATE_3_4_13_14_23_24(x))
#define FLAG_FIRST      0x1
#define FLAG_LAST       0x2
#define FLAG_MIDDLE     0x0

typedef enum targetPowerVHTRates {
    VHT_TARGET_RATE_0 = 0,
    VHT_TARGET_RATE_1_2,
    VHT_TARGET_RATE_3_4,
    VHT_TARGET_RATE_5,
    VHT_TARGET_RATE_6,
    VHT_TARGET_RATE_7,
    VHT_TARGET_RATE_8,
    VHT_TARGET_RATE_9,
    VHT_TARGET_RATE_10,
    VHT_TARGET_RATE_11_12,
    VHT_TARGET_RATE_13_14,
    VHT_TARGET_RATE_15,
    VHT_TARGET_RATE_16,
    VHT_TARGET_RATE_17,
    VHT_TARGET_RATE_18,
    VHT_TARGET_RATE_19,
    VHT_TARGET_RATE_20,
    VHT_TARGET_RATE_21_22,
    VHT_TARGET_RATE_23_24,
    VHT_TARGET_RATE_25,
    VHT_TARGET_RATE_26,
    VHT_TARGET_RATE_27,
    VHT_TARGET_RATE_28,
    VHT_TARGET_RATE_29,
    VHT_TARGET_RATE_LAST,
}TARGET_POWER_VHT_RATES;

enum BAND_enum
{
        band_undefined = -1,
        band_BG = 0,
        band_A,
        band_both,
};

#define ERR_EEP_READ            -5
#define ERR_MAX_REACHED         -4
#define ERR_NOT_FOUND           -3
#define ERR_VALUE_BAD           -2
#define ERR_RETURN              -1
#define VALUE_OK                0
#define VALUE_NEW               1
#define VALUE_UPDATED           2
#define VALUE_SAME              3
#define VALUE_REMOVED           4


#define IS_1STREAM_TARGET_POWER_VHT_RATES(x) (((x) >= VHT_TARGET_RATE_0) && ((x) <= VHT_TARGET_RATE_9))
#define IS_2STREAM_TARGET_POWER_VHT_RATES(x) (((x) >= VHT_TARGET_RATE_10 && (x) <= VHT_TARGET_RATE_19)) 
#define IS_3STREAM_TARGET_POWER_VHT_RATES(x) (((x) >= VHT_TARGET_RATE_20 && (x) <= VHT_TARGET_RATE_29) )

extern int ol_if_ratepwr_eeprom_to_usr(u_int8_t *eep_tbl, u_int16_t eep_len,
                                       u_int8_t *usr_tbl, u_int16_t usr_len);
extern int ol_if_ratepwr_usr_to_eeprom(u_int8_t *eep_tbl, u_int16_t eep_len,
                                       u_int8_t *usr_tbl, u_int16_t usr_len);
extern void ol_if_ratepwr_recv_buf(u_int8_t *ratepwr_tbl, u_int16_t ratepwr_len,
                                   u_int8_t frag_id, u_int8_t more_frag);
extern void ol_if_eeprom_attach(struct ieee80211com *ic);
#endif  /* __OL_IF_EEPROM_H__ */

