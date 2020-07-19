#if !defined(__CMD_TPCCAL_HANDLER_H)
#define __CMD_TPCCAL_HANDLER_H

// TX TPCCAL cmd parm struct
typedef struct tpccal_parms {
    A_UINT8    radioId;
    A_UINT8    pad[3];
} CMD_TPCCAL_PARMS;

#if !defined(WHAL_NUM_11G_CAL_PIERS)
#define WHAL_NUM_11G_CAL_PIERS  14
#define WHAL_NUM_11A_CAL_PIERS  32
#endif
#if !defined(MAX_NUM_CHAINS)
#define MAX_NUM_CHAINS           4
#endif //!defined(MAX_NUM_CHAINS)
#if !defined(NUM_CAL_GAINS_2G)
#define NUM_CAL_GAINS_2G        13
#define NUM_CAL_GAINS_5G        13
#endif

typedef struct tpccalrsp_parms {
    A_UINT8    radioId;                           // 1
    A_UINT8    chanFreq;                          // 1
    A_UINT8    numFreq2G;                         // 1
    A_UINT8    freq2G[WHAL_NUM_11G_CAL_PIERS];    // 14   17
    A_UINT8    numFreq5G;                         // 1
    A_UINT8    freq5G[WHAL_NUM_11A_CAL_PIERS];    // 32   50
    A_UINT8    numChain;                          // 1
    A_UINT8    chainMasks[MAX_NUM_CHAINS];        // 1
    A_UINT8    numCALPt2G;                        // 1    53
    A_INT16    tgtPwr2G[NUM_CAL_GAINS_2G];        // 26   79
    A_UINT8    CALPt2G[NUM_CAL_GAINS_2G];         // 13   92
    A_UINT8    txGains2G[NUM_CAL_GAINS_2G];       // 13  105
    A_UINT8    dacGains2G[NUM_CAL_GAINS_2G];      // 13  118
    A_UINT8    paCfg2G[NUM_CAL_GAINS_2G];         // 13  131
    A_UINT8    numCALPt5G;                        // 1   134
    A_UINT8    CALPt5G[NUM_CAL_GAINS_5G];         // 13  147
    A_INT16    tgtPwr5G[NUM_CAL_GAINS_5G];        // 13  160
    A_UINT8    txGains5G[NUM_CAL_GAINS_5G];       // 13  173
    A_UINT8    dacGains5G[NUM_CAL_GAINS_5G];      // 13  186
    A_UINT8    paCfg5G[NUM_CAL_GAINS_5G];         // 13  199
    A_UINT8    miscFlags;                         // 1   200
} CMD_TPCCALRSP_PARMS;

typedef void (*TPCCAL_OP_FUNC)(CMD_TPCCAL_PARMS *pParms);
typedef void (*TPCCALRSP_OP_FUNC)(CMD_TPCCALRSP_PARMS *pParms);

// Exposed functions
void* initTPCCALOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL TPCCALOp(void *pParms);

void* initTPCCALRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL TPCCALRSPOp(void *pParms);

#endif //#if !defined(__CMD_TPCCAL_HANDLER_H)

