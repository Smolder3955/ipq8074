#if !defined(__CMD_TPCCALPWR_HANDLER_H)
#define __CMD_TPCCALPWR_HANDLER_H

#if !defined(WHAL_NUM_11G_CAL_PIERS)
#define WHAL_NUM_11G_CAL_PIERS  14
#define WHAL_NUM_11A_CAL_PIERS  32
#endif
#if !defined(NUM_CAL_GAINS_2G)
#define NUM_CAL_GAINS_2G        13
#define NUM_CAL_GAINS_5G        13
#endif
#if !defined(MAX_NUM_CHAINS)
#define MAX_NUM_CHAINS  4
#endif
// TX TPCCAL cmd parm struct
#define NUM_MEASURED_PWR 512
typedef struct tpccalpwr_parms {
    A_INT16    measuredPwr[NUM_MEASURED_PWR];
    A_UINT8    radioId;
    A_UINT8    numMeasuredPwr;
    A_UINT8    pad[2];
} CMD_TPCCALPWR_PARMS;

typedef struct tpccaldata_parms {
    A_UINT8    miscFlags;                        //  1B  - FJC - Moved this to top of struct qmsl was having issue with it at bottom.
    A_UINT8    calData2G[256];                   // 64B
    A_UINT16   calData2GOffset;                  //  2B
    A_UINT16   calData2GLength;                  //  2B
    A_UINT8    calData5G[512];                    // 64B   132
    A_UINT16   calData5GOffset;                  //  2B   134 
    A_UINT16   calData5GLength;                  //  2B   136
    A_UINT8    calData2G_clpc[256];               // 64B   416
    A_UINT16   calData2GOffset_clpc;             //  2B   418
    A_UINT16   calData2GLength_clpc;             //  2B   420
    A_UINT8    calData5G_clpc[512];               // 64B   484
    A_UINT16   calData5GOffset_clpc;             //  2B   486
    A_UINT16   calData5GLength_clpc;             //  2B   488
    A_UINT8    radioId;                          //  1B
    A_UINT8    numFreq2G;                         // 1B   138
    A_UINT8    freq2G[WHAL_NUM_11G_CAL_PIERS];    // 14B  152
    A_UINT8    numFreq5G;                         // 1    156
    A_UINT8    freq5G[WHAL_NUM_11A_CAL_PIERS];    // 32   188
    A_UINT8    numChain;                          // 1    189
    A_UINT8    chainMasks[MAX_NUM_CHAINS];        // 1    193
    A_UINT8    numCALPt2G;                       //  1B   194
    A_INT16    tgtPwr2G[NUM_CAL_GAINS_2G];       // 26B   220
    A_UINT8    CALPt2G[NUM_CAL_GAINS_2G];        // 13B   233
    A_UINT8    txGains2G[NUM_CAL_GAINS_2G];      // 13B   246
    A_UINT8    dacGains2G[NUM_CAL_GAINS_2G];     // 13B   259
    A_UINT8    paCfg2G[NUM_CAL_GAINS_2G];        // 13B   272
    A_UINT8    numCALPt5G;                       //  1B   273
    A_INT16    tgtPwr5G[NUM_CAL_GAINS_2G];       // 26B   299
    A_UINT8    CALPt5G[NUM_CAL_GAINS_5G];        // 13B   312
    A_UINT8    txGains5G[NUM_CAL_GAINS_5G];      // 13B   325
    A_UINT8    dacGains5G[NUM_CAL_GAINS_5G];     // 13B   338
    A_UINT8    paCfg5G[NUM_CAL_GAINS_5G];        // 13B   351
} CMD_TPCCALDATA_PARMS;


typedef void (*TPCCALPWR_OP_FUNC)(CMD_TPCCALPWR_PARMS *pParms);
typedef void (*TPCCALDATA_OP_FUNC)(CMD_TPCCALDATA_PARMS *pParms);

// Exposed functions
void* initTPCCALPWROpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL TPCCALPWROp(void *pParms);

void* initTPCCALDATAOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL TPCCALDATAOp(void *pParms);

#endif //#if !defined(__CMD_TPCCALPWR_HANDLER_H)

