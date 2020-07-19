/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _PKTLOG_RC_H_
#define _PKTLOG_RC_H_

#include "pktlog.h"

struct log_rcfind{
    struct TxRateCtrl_s *rc;
    u_int8_t rate;
    u_int8_t rateCode;
    int8_t rssiReduce;
    int8_t isProbing;
    int8_t primeInUse;
    int8_t currentPrimeState;
    u_int8_t ac;
    int32_t misc[8]; /* Can be used for HT specific or other misc info */
    /* TBD: Add other required log information */
};

struct log_rcupdate{
    struct TxRateCtrl_s *rc;
    int8_t currentBoostState;
    int8_t useTurboPrime;
    u_int8_t txRate;
    u_int8_t rateCode;
    u_int8_t Xretries;
    u_int8_t retries;
    int8_t   rssiAck;
    u_int8_t ac;
    int32_t misc[8]; /* Can be used for HT specific or other misc info */
    /* TBD: Add other required log information */
};

struct ath_pktlog_rcfuncs {
    void (*pktlog_rcfind)(struct ath_softc *, struct log_rcfind *, u_int32_t);
    void (*pktlog_rcupdate)(struct ath_softc *, struct log_rcupdate *, u_int32_t);
};

#define ath_log_rcfind(_sc, _log_data, _flags)                          \
    do {                                                                \
        if (g_pktlog_rcfuncs) {                                         \
            g_pktlog_rcfuncs->pktlog_rcfind(_sc, _log_data, _flags);    \
        }                                                               \
    } while(0)
                
#define ath_log_rcupdate(_sc, _log_data, _flags)                        \
    do {                                                                \
        if (g_pktlog_rcfuncs) {                                         \
            g_pktlog_rcfuncs->pktlog_rcupdate(_sc, _log_data, _flags);  \
        }                                                               \
    } while(0)

#endif /* _PKTLOG_RC_H_ */
