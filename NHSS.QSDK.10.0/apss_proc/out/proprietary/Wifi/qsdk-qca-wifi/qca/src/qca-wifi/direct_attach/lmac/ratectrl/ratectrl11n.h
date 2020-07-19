/*
 * Copyright (c) 2000-2002, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 *
 * Definitions for core driver
 * This is a common header file for all platforms and operating systems.
 */
#ifndef _RATECTRL11N_H_
#define _RATECTRL11N_H_

#include "ratectrl.h" /* A_UINT32, etc. */
#include "ah.h"
/* target PER for rate control */
#define ATH_RATE_TGT_PER_PCT      12
#define ATH_RATE_TGT_PER_PCT_VIVO 12
/* HT 20/40 rates. If 20 bit is enabled then that rate is
 * used only in 20 mode. If both 20/40 bits are enabled
 * then that rate can be used for both 20 and 40 mode */

#define TRUE_20         0x2
#define TRUE_40     0x4
#define TRUE_2040    (TRUE_20|TRUE_40)
#define TRUE_ALL    (TRUE_2040|TRUE)

enum {
    WLAN_RC_DS  = 0x01,
    WLAN_RC_40  = 0x02,
    WLAN_RC_SGI = 0x04,
    WLAN_RC_HT  = 0x08,
};

typedef enum {
    WLAN_RC_LEGACY      = 0,
    WLAN_RC_HT_LNPHY    = 1,
    WLAN_RC_HT_PLPHY    = 2,
    WLAN_RC_MAX         = 3
} WLAN_RC_VERS;


#define WLAN_RC_PHY_SS(_phy)   ((_phy == WLAN_RC_PHY_HT_20_SS)           \
                                || (_phy == WLAN_RC_PHY_HT_40_SS)        \
                                || (_phy == WLAN_RC_PHY_HT_20_SS_HGI)    \
                                || (_phy == WLAN_RC_PHY_HT_40_SS_HGI))
#define WLAN_RC_PHY_DS(_phy)   ((_phy == WLAN_RC_PHY_HT_20_DS)           \
                                || (_phy == WLAN_RC_PHY_HT_40_DS)        \
                                || (_phy == WLAN_RC_PHY_HT_20_DS_HGI)    \
                                || (_phy == WLAN_RC_PHY_HT_40_DS_HGI))
#define WLAN_RC_PHY_40(_phy)   ((_phy == WLAN_RC_PHY_HT_40_SS)           \
                                || (_phy == WLAN_RC_PHY_HT_40_DS)        \
                                || (_phy == WLAN_RC_PHY_HT_40_TS)        \
                                || (_phy == WLAN_RC_PHY_HT_40_SS_HGI)    \
                                || (_phy == WLAN_RC_PHY_HT_40_DS_HGI)    \
                                || (_phy == WLAN_RC_PHY_HT_40_TS_HGI))
#define WLAN_RC_PHY_SGI(_phy)  ((_phy == WLAN_RC_PHY_HT_20_SS_HGI)      \
                                || (_phy == WLAN_RC_PHY_HT_20_DS_HGI)   \
                                || (_phy == WLAN_RC_PHY_HT_20_TS_HGI)   \
                                || (_phy == WLAN_RC_PHY_HT_40_SS_HGI)   \
                                || (_phy == WLAN_RC_PHY_HT_40_DS_HGI)   \
                                || (_phy == WLAN_RC_PHY_HT_40_TS_HGI))
#define WLAN_RC_PHY_TS(_phy)   ((_phy == WLAN_RC_PHY_HT_20_TS)           \
                                || (_phy == WLAN_RC_PHY_HT_40_TS)        \
                                || (_phy == WLAN_RC_PHY_HT_20_TS_HGI)    \
                                || (_phy == WLAN_RC_PHY_HT_40_TS_HGI))

#define WLAN_RC_PHY_HT(_phy)    (_phy >= WLAN_RC_PHY_HT_20_SS)

/* Returns the capflag mode */
#define WLAN_RC_CAP_MODE(capflag) (((capflag & WLAN_RC_HT_FLAG)?    \
                    (capflag & WLAN_RC_40_FLAG)?TRUE_40:TRUE_20:\
                    TRUE))

/* Return TRUE if flag supports HT20 && client supports HT20 or
 * return TRUE if flag supports HT40 && client supports HT40.
 * This is used becos some rates overlap between HT20/HT40.
 */

#define WLAN_RC_PHY_HT_VALID(flag, capflag) (((flag & TRUE_20) && !(capflag \
                & WLAN_RC_40_FLAG)) || ((flag & TRUE_40) && \
                  (capflag & WLAN_RC_40_FLAG)))


#define WLAN_RC_DS_FLAG         (0x01)
#define WLAN_RC_40_FLAG         (0x02)
#define WLAN_RC_SGI_FLAG        (0x04)
#define WLAN_RC_HT_FLAG         (0x08)
#define WLAN_RC_STBC_FLAG       (0x30)  /* 2 bits */
#define WLAN_RC_STBC_FLAG_S     (   4)
#define WLAN_RC_TS_FLAG         (0x200)
#define WLAN_RC_TxBF_FLAG       (0x400)
#define WLAN_RC_CEC_FLAG        (0x1800) /*CEC (Channel Estimation Capability) ,2 bits */
#define WLAN_RC_CEC_FLAG_S      (    11)

/* flag for TxBF */
/*
 *  N2;N1 mean Nss(number of spatial streams) of Beamforming
 *  Nss=2 : dualStream & tripleStream mode (DS & TS)
 *  Nss=1 : singleStream ,dualStream mode & tripleStream mode (SS, DS & TS)
 *  
 *  TRUE_AS_NB
 *  A: which stream mode supported (TS, DS or SS)
 *  B: under which Nss number
 */

#define TRUE_TS_N2              (0x01)
#define TRUE_DS_N2              (0x02)
#define TRUE_TS_DS_N2           (TRUE_TS_N2|TRUE_DS_N2)
#define TRUE_TS_N1              (0x04)
#define TRUE_DS_N1              (0x08)
#define TRUE_SS_N1              (0x10)
#define TXBF_TRUE20             (0x20)
#define TXBF_TRUE40             (0x40)
#define TXBF_TRUE20_40          (TXBF_TRUE20|TXBF_TRUE40)
#define TRUE_TS_SS_N1           (TRUE_TS_N1|TRUE_SS_N1)
#define TRUE_DS_SS_N1           (TRUE_DS_N1|TRUE_SS_N1)
#define TRUE_TS_DS_N1           (TRUE_TS_N1|TRUE_DS_N1)
#define TRUE_ALL_N1             (TRUE_TS_N1|TRUE_DS_N1|TRUE_SS_N1)

#define WLAN_RC_PHY_TxBF_HT_VALID(flag, capflag) (((flag & TXBF_TRUE20) && !(capflag \
                & WLAN_RC_40_FLAG)) || ((flag & TXBF_TRUE40) && \
                  (capflag & WLAN_RC_40_FLAG)))

#define VALID_TXBF_RATE(_rate,_nss) ((_rate >= 0x80) && (_rate < ((_nss == 2)?0x90:0x88)))

#define LEGACY_RATE(_rate)((_rate) < 0x80)
#define TS_RATE(_rate) ((_rate) >= 0x90)
#define TS_SOUNDING_SWAP_RATE   0x8a
#define MCS0    0x80
#define MCS0_80_PERCENT 200


#define SWAP_RATE           0xf
#define SWAP_TABLE_SIZE     SWAP_RATE+1 
extern const u_int8_t sounding_swap_table_A_40[SWAP_TABLE_SIZE];
extern const u_int8_t sounding_swap_table_A_20[SWAP_TABLE_SIZE];
extern const u_int8_t sounding_swap_table_G_40[SWAP_TABLE_SIZE];
extern const u_int8_t sounding_swap_table_G_20[SWAP_TABLE_SIZE];
                                                    
/* valid rate defines*/
#define VALID_RATE_NoTxBF(_pRateTable,_pSib,_rate)  \
	((_pRateTable->info[_rate].validSS && _pSib->singleStream) ||                            \
     (_pRateTable->info[_rate].validDS && _pSib->dualStream) ||                              \
     (_pRateTable->info[_rate].validTS && _pSib->tripleStream) ||                            \
     (_pRateTable->info[_rate].validSTBC_SS && (_pSib->stbc && _pSib->singleStream)) ||       \
     (_pRateTable->info[_rate].validSTBC_DS && (_pSib->stbc && _pSib->dualStream)) ||         \
     (_pRateTable->info[_rate].validSTBC_TS && (_pSib->stbc && _pSib->tripleStream)))

#ifdef ATH_SUPPORT_TxBF
#define VALID_RATE_TXBF_N2(_pRateTable,_pSib,_rate)  \
     (((_pRateTable->info[_rate].validTxBF & TRUE_TS_N2) &&                 \
         (_pSib->txbf && _pSib->tripleStream)) ||                           \
     ((_pRateTable->info[_rate].validTxBF & TRUE_DS_N2) &&                  \
         (_pSib->txbf && _pSib->dualStream)))

#define VALID_RATE_TXBF_N1(_pRateTable,_pSib,_rate)  \
     (((_pRateTable->info[_rate].validTxBF & TRUE_TS_N1) &&                 \
         (_pSib->txbf && _pSib->tripleStream)) ||                           \
     ((_pRateTable->info[_rate].validTxBF & TRUE_DS_N1) &&                  \
         (_pSib->txbf && _pSib->dualStream)) ||                             \
     ((_pRateTable->info[_rate].validTxBF & TRUE_SS_N1) &&                  \
         (_pSib->txbf && _pSib->singleStream)))

#define NSS_DIM_2 0x2
#define NSS_DIM_1 0x1
#define VALID_RATE(_pRateTable,_pSib,_rate)  \
    (VALID_RATE_NoTxBF(_pRateTable,_pSib,_rate) || ((_pSib->usedNss == NSS_DIM_2) ? VALID_RATE_TXBF_N2(_pRateTable,_pSib,_rate) : VALID_RATE_TXBF_N1(_pRateTable,_pSib,_rate)))
    
#define IS_TXBF_RATE_CONTROL(_pSib)     \
    ((_pSib->txbf_steered) && (_pSib->avp->athdev_vap->av_config.av_auto_cv_update))

static inline A_UINT8
rcIsValidTxBFPhyRat(struct atheros_node *pSib, const A_UINT8 flag)
{
    if (pSib->usedNss == NSS_DIM_2) {
        // for Nss Dimension to be 2
        if (pSib->tripleStream) { 
            if (flag & TRUE_TS_N2) {
                // for triple stream
                return true;
            }
        } else  if (pSib->dualStream) {
            if (flag & TRUE_DS_N2) {
                // for dual stream
                return true;
            }
        }
    } else if (pSib->usedNss == NSS_DIM_1) {
        // for Nss Dimension to be 1
        if (pSib->tripleStream) {
            if (flag & TRUE_TS_N1) {
                // for triple stream
                return true;
            }
        } else if (pSib->dualStream) {
            if (flag & TRUE_DS_N1) {
                // for dual stream
                return true;
            }
        } else  if (pSib->singleStream) { 
            if (flag & TRUE_SS_N1) {
                // for single stream
                return true;
            }
        }
    }
    return false;
}
    
#else
#define VALID_RATE(_pRateTable,_pSib,_rate)  \
        VALID_RATE_NoTxBF(_pRateTable,_pSib,_rate)
#define IS_TXBF_RATE_CONTROL(_pSib) 0
#endif

/* Index into the rate table */
#define INIT_RATE_MAX_20    23
#define INIT_RATE_MAX_40    40

/*                         One       Two      Three
 *                        strm      strm      strm
 */
#define TRUE_ALL_1_2_3  TRUE_ALL, TRUE_ALL, TRUE_ALL
#define FALSE_1_2_3        FALSE,    FALSE,    FALSE
#define TRUE_1_2_3          TRUE,     TRUE,     TRUE
#define TRUE20_1_2_3     TRUE_20,  TRUE_20,  TRUE_20
#define TRUE20_1         TRUE_20,    FALSE,    FALSE
#define TRUE20_2           FALSE,  TRUE_20,    FALSE
#define TRUE20_3           FALSE,    FALSE,  TRUE_20
#define TRUE20_1_2       TRUE_20,  TRUE_20,    FALSE
#define TRUE20_2_3         FALSE,  TRUE_20,  TRUE_20
#define TRUE40_1_2_3     TRUE_40,  TRUE_40,  TRUE_40
#define TRUE40_1         TRUE_40,    FALSE,    FALSE
#define TRUE40_2           FALSE,  TRUE_40,    FALSE
#define TRUE40_3           FALSE,    FALSE,  TRUE_40
#define TRUE40_1_2       TRUE_40,  TRUE_40,    FALSE
#define TRUE40_2_3         FALSE,  TRUE_40,  TRUE_40
#define TRUE2040_1_2_3 (TRUE_20|TRUE_40), (TRUE_20|TRUE_40) ,(TRUE_20|TRUE_40)    
#define TRUE2040       (TRUE_20|TRUE_40)



/*                          
 * TRUE_N2_A_N1_B
 * A: support under Nss=2
 * B: support under Nss=1
 *      ALL     : rate supported for all configuration of its Nss
 *      S,D,T   : rate supported for single, dual ,triple stream of its Nss
 *      F       : rate not supported
 *
 *                          (Nss2   |   Nss1)   
 */
#define TRUE_N_1_2_ALL      (TRUE_TS_DS_N2|TRUE_ALL_N1)
#define FALSE_N_ALL         (FALSE|FALSE)
#define TRUE_N2_ALL_N1_F    (TRUE_TS_DS_N2|FALSE)
#define TRUE_N2_T_N1_F      (TRUE_TS_N2|FALSE)
#define TRUE_N2_D_N1_F      (TRUE_DS_N2|FALSE)
#define TRUE_N2_ALL_N1_T    (TRUE_TS_DS_N2|TRUE_TS_N1)
#define TRUE_N2_T_N1_T      (TRUE_TS_N2|TRUE_TS_N1)
#define TRUE_N2_D_N1_T      (TRUE_DS_N2|TRUE_TS_N1)
#define TRUE_N2_F_N1_T      (FALSE|TRUE_TS_N1)
#define TRUE_N2_ALL_N1_D    (TRUE_TS_DS_N2|TRUE_DS_N1)
#define TRUE_N2_T_N1_D      (TRUE_TS_N2|TRUE_DS_N1)
#define TRUE_N2_D_N1_D      (TRUE_DS_N2|TRUE_DS_N1)
#define TRUE_N2_F_N1_D      (FALSE|TRUE_DS_N1)
#define TRUE_N2_ALL_N1_S    (TRUE_TS_DS_N2 | TRUE_SS_N1)
#define TRUE_N2_T_N1_S      (TRUE_TS_N2 | TRUE_SS_N1)
#define TRUE_N2_D_N1_S      (TRUE_DS_N2 | TRUE_SS_N1)
#define TRUE_N2_F_N1_S      (FALSE | TRUE_SS_N1)
#define TRUE_N2_ALL_N1_T_S  (TRUE_TS_DS_N2 | TRUE_TS_SS_N1)
#define TRUE_N2_T_N1_T_S    (TRUE_TS_N2 | TRUE_TS_SS_N1)
#define TRUE_N2_D_N1_T_S    (TRUE_DS_N2 | TRUE_TS_SS_N1)
#define TRUE_N2_F_N1_T_S    (FALSE | TRUE_TS_SS_N1)
#define TRUE_N2_ALL_N1_D_S  (TRUE_TS_DS_N2 | TRUE_DS_SS_N1)
#define TRUE_N2_T_N1_D_S    (TRUE_TS_N2 | TRUE_DS_SS_N1)
#define TRUE_N2_D_N1_D_S    (TRUE_DS_N2 | TRUE_DS_SS_N1)
#define TRUE_N2_F_N1_D_S    (FALSE | TRUE_DS_SS_N1)
#define TRUE_N2_ALL_N1_T_D  (TRUE_TS_DS_N2 | TRUE_TS_DS_N1)
#define TRUE_N2_T_N1_T_D    (TRUE_TS_N2 | TRUE_TS_DS_N1)
#define TRUE_N2_D_N1_T_D    (TRUE_DS_N2 | TRUE_TS_DS_N1)
#define TRUE_N2_F_N1_T_D    (FALSE | TRUE_TS_DS_N1)
#define TRUE_N2_T_N1_ALL    (TRUE_TS_N2 | TRUE_ALL_N1)
#define TRUE_N2_D_N1_ALL    (TRUE_DS_N2 | TRUE_ALL_N1)
#define TRUE_N2_F_N1_ALL    (FALSE | TRUE_ALL_N1)

#define TRUE20_N_1_2_ALL        (TXBF_TRUE20|TRUE_N_1_2_ALL)
#define TRUE20_N2_ALL_N1_F      (TXBF_TRUE20|TRUE_N2_ALL_N1_F)
#define TRUE20_N2_T_N1_F        (TXBF_TRUE20|TRUE_N2_T_N1_F)
#define TRUE20_N2_D_N1_F        (TXBF_TRUE20|TRUE_N2_D_N1_F)
#define TRUE20_N2_ALL_N1_T      (TXBF_TRUE20|TRUE_N2_ALL_N1_T)
#define TRUE20_N2_T_N1_T        (TXBF_TRUE20|TRUE_N2_T_N1_T)
#define TRUE20_N2_D_N1_T        (TXBF_TRUE20|TRUE_N2_D_N1_T)
#define TRUE20_N2_F_N1_T        (TXBF_TRUE20|TRUE_N2_F_N1_T)
#define TRUE20_N2_ALL_N1_D      (TXBF_TRUE20|TRUE_N2_ALL_N1_D)
#define TRUE20_N2_T_N1_D        (TXBF_TRUE20|TRUE_N2_T_N1_D)
#define TRUE20_N2_D_N1_D        (TXBF_TRUE20|TRUE_N2_D_N1_D)
#define TRUE20_N2_F_N1_D        (TXBF_TRUE20|TRUE_N2_F_N1_D)
#define TRUE20_N2_ALL_N1_S      (TXBF_TRUE20|TRUE_N2_ALL_N1_S)
#define TRUE20_N2_T_N1_S        (TXBF_TRUE20|TRUE_N2_T_N1_S)
#define TRUE20_N2_D_N1_S        (TXBF_TRUE20|TRUE_N2_D_N1_S)
#define TRUE20_N2_F_N1_S        (TXBF_TRUE20|TRUE_N2_F_N1_S)
#define TRUE20_N2_ALL_N1_T_S    (TXBF_TRUE20|TRUE_N2_ALL_N1_T_S)
#define TRUE20_N2_T_N1_T_S      (TXBF_TRUE20|TRUE_N2_T_N1_T_S)
#define TRUE20_N2_D_N1_T_S      (TXBF_TRUE20|TRUE_N2_D_N1_T_S)
#define TRUE20_N2_F_N1_T_S      (TXBF_TRUE20|TRUE_N2_F_N1_T_S)
#define TRUE20_N2_ALL_N1_D_S    (TXBF_TRUE20|TRUE_N2_ALL_N1_D_S)
#define TRUE20_N2_T_N1_D_S      (TXBF_TRUE20|TRUE_N2_T_N1_D_S)
#define TRUE20_N2_D_N1_D_S      (TXBF_TRUE20|TRUE_N2_D_N1_D_S)
#define TRUE20_N2_F_N1_D_S      (TXBF_TRUE20|TRUE_N2_F_N1_D_S)
#define TRUE20_N2_ALL_N1_T_D    (TXBF_TRUE20|TRUE_N2_ALL_N1_T_D)
#define TRUE20_N2_T_N1_T_D      (TXBF_TRUE20|TRUE_N2_T_N1_T_D)
#define TRUE20_N2_D_N1_T_D      (TXBF_TRUE20|TRUE_N2_D_N1_T_D)
#define TRUE20_N2_F_N1_T_D      (TXBF_TRUE20|TRUE_N2_F_N1_T_D)
#define TRUE20_N2_T_N1_ALL      (TXBF_TRUE20|TRUE_N2_T_N1_ALL)
#define TRUE20_N2_D_N1_ALL      (TXBF_TRUE20|TRUE_N2_D_N1_ALL)
#define TRUE20_N2_F_N1_ALL      (TXBF_TRUE20|TRUE_N2_F_N1_ALL)

#define TRUE40_N_1_2_ALL        (TXBF_TRUE40|TRUE_N_1_2_ALL)
#define TRUE40_N2_ALL_N1_F      (TXBF_TRUE40|TRUE_N2_ALL_N1_F)
#define TRUE40_N2_T_N1_F        (TXBF_TRUE40|TRUE_N2_T_N1_F)
#define TRUE40_N2_D_N1_F        (TXBF_TRUE40|TRUE_N2_D_N1_F)
#define TRUE40_N2_ALL_N1_T      (TXBF_TRUE40|TRUE_N2_ALL_N1_T)
#define TRUE40_N2_T_N1_T        (TXBF_TRUE40|TRUE_N2_T_N1_T)
#define TRUE40_N2_D_N1_T        (TXBF_TRUE40|TRUE_N2_D_N1_T)
#define TRUE40_N2_F_N1_T        (TXBF_TRUE40|TRUE_N2_F_N1_T)
#define TRUE40_N2_ALL_N1_D      (TXBF_TRUE40|TRUE_N2_ALL_N1_D)
#define TRUE40_N2_T_N1_D        (TXBF_TRUE40|TRUE_N2_T_N1_D)
#define TRUE40_N2_D_N1_D        (TXBF_TRUE40|TRUE_N2_D_N1_D)
#define TRUE40_N2_F_N1_D        (TXBF_TRUE40|TRUE_N2_F_N1_D)
#define TRUE40_N2_ALL_N1_S      (TXBF_TRUE40|TRUE_N2_ALL_N1_S)
#define TRUE40_N2_T_N1_S        (TXBF_TRUE40|TRUE_N2_T_N1_S)
#define TRUE40_N2_D_N1_S        (TXBF_TRUE40|TRUE_N2_D_N1_S)
#define TRUE40_N2_F_N1_S        (TXBF_TRUE40|TRUE_N2_F_N1_S)
#define TRUE40_N2_ALL_N1_T_S    (TXBF_TRUE40|TRUE_N2_ALL_N1_T_S)
#define TRUE40_N2_T_N1_T_S      (TXBF_TRUE40|TRUE_N2_T_N1_T_S)
#define TRUE40_N2_D_N1_T_S      (TXBF_TRUE40|TRUE_N2_D_N1_T_S)
#define TRUE40_N2_F_N1_T_S      (TXBF_TRUE40|TRUE_N2_F_N1_T_S)
#define TRUE40_N2_ALL_N1_D_S    (TXBF_TRUE40|TRUE_N2_ALL_N1_D_S)
#define TRUE40_N2_T_N1_D_S      (TXBF_TRUE40|TRUE_N2_T_N1_D_S)
#define TRUE40_N2_D_N1_D_S      (TXBF_TRUE40|TRUE_N2_D_N1_D_S)
#define TRUE40_N2_F_N1_D_S      (TXBF_TRUE40|TRUE_N2_F_N1_D_S)
#define TRUE40_N2_ALL_N1_T_D    (TXBF_TRUE40|TRUE_N2_ALL_N1_T_D)
#define TRUE40_N2_T_N1_T_D      (TXBF_TRUE40|TRUE_N2_T_N1_T_D)
#define TRUE40_N2_D_N1_T_D      (TXBF_TRUE40|TRUE_N2_D_N1_T_D)
#define TRUE40_N2_F_N1_T_D      (TXBF_TRUE40|TRUE_N2_F_N1_T_D)
#define TRUE40_N2_T_N1_ALL      (TXBF_TRUE40|TRUE_N2_T_N1_ALL)
#define TRUE40_N2_D_N1_ALL      (TXBF_TRUE40|TRUE_N2_D_N1_ALL)
#define TRUE40_N2_F_N1_ALL      (TXBF_TRUE40|TRUE_N2_F_N1_ALL)

#define TRUE2040_N_1_2_ALL      (TXBF_TRUE20_40|TRUE_N_1_2_ALL)
#define TRUE2040_N2_ALL_N1_F    (TXBF_TRUE20_40|TRUE_N2_ALL_N1_F)
#define TRUE2040_N2_T_N1_F      (TXBF_TRUE20_40|TRUE_N2_T_N1_F)
#define TRUE2040_N2_D_N1_F      (TXBF_TRUE20_40|TRUE_N2_D_N1_F)
#define TRUE2040_N2_ALL_N1_T    (TXBF_TRUE20_40|TRUE_N2_ALL_N1_T)
#define TRUE2040_N2_T_N1_T      (TXBF_TRUE20_40|TRUE_N2_T_N1_T)
#define TRUE2040_N2_D_N1_T      (TXBF_TRUE20_40|TRUE_N2_D_N1_T)
#define TRUE2040_N2_F_N1_T      (TXBF_TRUE20_40|TRUE_N2_F_N1_T)
#define TRUE2040_N2_ALL_N1_D    (TXBF_TRUE20_40|TRUE_N2_ALL_N1_D)
#define TRUE2040_N2_T_N1_D      (TXBF_TRUE20_40|TRUE_N2_T_N1_D)
#define TRUE2040_N2_D_N1_D      (TXBF_TRUE20_40|TRUE_N2_D_N1_D)
#define TRUE2040_N2_F_N1_D      (TXBF_TRUE20_40|TRUE_N2_F_N1_D)
#define TRUE2040_N2_ALL_N1_S    (TXBF_TRUE20_40|TRUE_N2_ALL_N1_S)
#define TRUE2040_N2_T_N1_S      (TXBF_TRUE20_40|TRUE_N2_T_N1_S)
#define TRUE2040_N2_D_N1_S      (TXBF_TRUE20_40|TRUE_N2_D_N1_S)
#define TRUE2040_N2_F_N1_S      (TXBF_TRUE20_40|TRUE_N2_F_N1_S)
#define TRUE2040_N2_ALL_N1_T_S  (TXBF_TRUE20_40|TRUE_N2_ALL_N1_T_S)
#define TRUE2040_N2_T_N1_T_S    (TXBF_TRUE20_40|TRUE_N2_T_N1_T_S)
#define TRUE2040_N2_D_N1_T_S    (TXBF_TRUE20_40|TRUE_N2_D_N1_T_S)
#define TRUE2040_N2_F_N1_T_S    (TXBF_TRUE20_40|TRUE_N2_F_N1_T_S)
#define TRUE2040_N2_ALL_N1_D_S  (TXBF_TRUE20_40|TRUE_N2_ALL_N1_D_S)
#define TRUE2040_N2_T_N1_D_S    (TXBF_TRUE20_40|TRUE_N2_T_N1_D_S)
#define TRUE2040_N2_D_N1_D_S    (TXBF_TRUE20_40|TRUE_N2_D_N1_D_S)
#define TRUE2040_N2_F_N1_D_S    (TXBF_TRUE20_40|TRUE_N2_F_N1_D_S)
#define TRUE2040_N2_ALL_N1_T_D  (TXBF_TRUE20_40|TRUE_N2_ALL_N1_T_D)
#define TRUE2040_N2_T_N1_T_D    (TXBF_TRUE20_40|TRUE_N2_T_N1_T_D)
#define TRUE2040_N2_D_N1_T_D    (TXBF_TRUE20_40|TRUE_N2_D_N1_T_D)
#define TRUE2040_N2_F_N1_T_D    (TXBF_TRUE20_40|TRUE_N2_F_N1_T_D)
#define TRUE2040_N2_T_N1_ALL    (TXBF_TRUE20_40|TRUE_N2_T_N1_ALL)
#define TRUE2040_N2_D_N1_ALL    (TXBF_TRUE20_40|TRUE_N2_D_N1_ALL)
#define TRUE2040_N2_F_N1_ALL    (TXBF_TRUE20_40|TRUE_N2_F_N1_ALL)

#define RATE_TABLE_SIZE_11N             72
typedef struct {
        A_UINT32  validSS      : 4, /* Valid for use in rate control for single stream operation */
                  validDS      : 4, /* Valid for use in rate control for dual stream operation */
                  validTS      : 4, /* Valid for use in rate control for triple stream operation */
                  validSTBC_SS : 4, /* Valid for use in rate control for STBC operation on 1 stream products */
                  validSTBC_DS : 4, /* Valid for use in rate control for STBC operation on 2 stream products */
                  validSTBC_TS : 4, /* Valid for use in rate control for STBC operation on 3 stream products */
                  validUAPSD   : 1, /* Valid for use in rate control for UAPSD operation */
                  validTxBF    : 7; /* Valid for use in rate control for Beamforming operation */
        WLAN_PHY  phy;                /* CCK/OFDM/TURBO/XR */
        A_UINT32  rateKbps;           /* Rate in Kbits per second */
        A_UINT32  userRateKbps;       /* User rate in KBits per second */
        A_UINT8   rateCode;           /* rate that goes into hw descriptors */
        A_UINT8   shortPreamble;      /* Mask for enabling short preamble in rate code for CCK */
        A_UINT8   dot11Rate;          /* Value that goes into supported rates info element of MLME */
        A_UINT8   controlRate;        /* Index of next lower basic rate, used for duration computation */
        A_RSSI    rssiAckValidMin;    /* Rate control related information */
        A_RSSI    rssiAckDeltaMin;    /* Rate control related information */
        A_UINT8   maxTxChains;        /* maximum number of tx chains to use */
        A_UINT8   baseIndex;          /* base rate index */
        A_UINT8   cw40Index;          /* 40cap rate index */
        A_UINT8   sgiIndex;           /* shortgi rate index */
        A_UINT8   htIndex;            /* shortgi rate index */
        A_UINT32  max4msframelen;     /* Maximum frame length(bytes) for 4ms tx duration */
} RATE_INFO_11N;

typedef struct {
    int            rateCount;
    RATE_INFO_11N  *info;
    A_UINT32       probeInterval;        /* interval for ratectrl to probe for other rates */
    A_UINT32       rssiReduceInterval;   /* interval for ratectrl to reduce RSSI */
    A_UINT8        initialRateMax;       /* the initial rateMax value used in rcSibUpdate() */
} RATE_TABLE_11N;


#define ATH_NODE_ATHEROS(_an)         (_an)->an_rc_node

typedef struct _MAX_RATES {
    A_UINT32 max_ht20_tx_rateKbps;
    A_UINT32 max_ht40_tx_rateKbps;
} MAX_RATES;

#if ATH_SUPPORT_IQUE
/* Below def should be in sync with 'ieee80211_hbr_event' def in umac */
typedef enum {
    HBR_EVENT_BACK,
    HBR_EVENT_FORWARD,
    HBR_EVENT_STALE,
}hbr_event;
#endif

/*
 *  Update the SIB's rate control information
 *
 *  This should be called when the supported rates change
 *  (e.g. SME operation, wireless mode change)
 *
 *  It will determine which rates are valid for use.
 */
void
rcSibUpdate_11n(struct ath_softc *, struct ath_node *, A_UINT32 capflag,
            int keepState, struct ieee80211_rateset *negotiated_rates,
                        struct ieee80211_rateset *negotiated_htrates,
                        const MAX_RATES *maxRates);
/*
 *  This routine is called to initialize the rate control parameters
 *  in the SIB. It is called initially during system initialization
 *  or when a station is associated with the AP.
 */
void    rcSibInit_11n(struct ath_softc *, struct ath_node *);

/*
 * Determines and returns the new Tx rate index.
 */
void
rcRateFind_11n(struct ath_softc *sc, struct ath_node *an, A_UINT8 ac,
              int numTries, int numRates, unsigned int rcflag,
              struct ath_rc_series series[], int *isProbe, int isretry);

#if UNIFIED_SMARTANTENNA
void
rcSmartAnt_SetRate_11n(struct ath_softc *sc, struct ath_node *an,
              struct ath_rc_series series[], u_int8_t *rate_array, u_int32_t *antenna_array, u_int32_t rc_flags);
#endif

/*
 * This routine is called by the Tx interrupt service routine to give
 * the status of previous frames.
 */
#ifdef ATH_SUPPORT_VOWEXT
void
rcUpdate_11n(struct ath_softc *sc, struct ath_node *an,
            A_RSSI rssiAck, A_UINT8 ac, int finalTSIdx, int Xretries, struct ath_rc_series rcs[],
            int nFrames, int nBad, int long_retry, int short_retry_fail, int nHeadFail, int nTailFail);
#else
void
rcUpdate_11n(struct ath_softc *sc, struct ath_node *an,
            A_RSSI rssiAck, A_UINT8 ac, int finalTSIdx, int Xretries, struct ath_rc_series rcs[],
            int nFrames, int nBad, int long_retry, int short_retry_fail);
#endif
#if ATH_SUPPORT_IQUE
void
rcSibUpdate_11nViVo(struct ath_softc *, struct ath_node *, A_UINT32 capflag,
                int keepState, struct ieee80211_rateset *negotiated_rates,
                struct ieee80211_rateset *negotiated_htrates, const MAX_RATES *maxRates);

void
rcRateFind_11nViVo(struct ath_softc *sc, struct ath_node *an, A_UINT8 ac,
              int numTries, int numRates, unsigned int rcflag,
              struct ath_rc_series series[], int *isProbe, int isretry);

#if ATH_SUPPORT_VOWEXT
void
rcUpdate_11nViVo(struct ath_softc *sc, struct ath_node *an,
            A_RSSI rssiAck, A_UINT8 ac, int finalTSIdx, int Xretries, struct ath_rc_series rcs[],
            int nFrames, int nBad, int long_retry, int short_retry_fail, int nHeadFail, int nTailFail);
#else
void
rcUpdate_11nViVo(struct ath_softc *sc, struct ath_node *an,
            A_RSSI rssiAck, A_UINT8 ac, int finalTSIdx, int Xretries, struct ath_rc_series rcs[],
            int nFrames, int nBad, int long_retry, int short_retry_fail);
#endif
#endif /* ATH_SUPPORT_IQUE */

#if ATH_CCX
u_int8_t
rcRateValueToPer_11n(struct ath_softc *sc, struct ath_node *an, int txRateKbps);
#endif /* ATH_CCX */

/*
 * Return valid flags based on the mode of operation.
 */
static inline
A_UINT32 getValidFlags(struct atheros_node *pSib, const RATE_TABLE_11N *pRateTable, A_UINT8 rate)
{
    if (pSib->singleStream) {
        return pRateTable->info[rate].validSS;
    } else if (pSib->stbc) {
        if (pSib->dualStream) {
            return pRateTable->info[rate].validSTBC_DS;
		} else if (pSib->tripleStream) {
            return pRateTable->info[rate].validSTBC_TS;
		} else {
			return pRateTable->info[rate].validSTBC_SS;
		}
    } else if (pSib->dualStream) {
        return pRateTable->info[rate].validDS;
    } else if (pSib->tripleStream) {
        return pRateTable->info[rate].validTS;
    } else {
        return FALSE;
    }
}

static inline void
rcSkipLGIRates(const RATE_TABLE_11N *pRateTable, TX_RATE_CTRL *pRc)
{
    /* 
     * According to System Engineer (Enis Akay), for radio 
     * property reason, non SGI rate should be skipped if 
     * corresponding SGI rate is valid. The rule should 
     * only apply to rate higher than or equal to MCS21.
     * Here these LGI rates are delete from the valid rate
     * index table.
     */

    A_UINT8 i,j;
    A_UINT8 rate;

    if (!pRc->maxValidRate) {
        return;
    }

    for (i = 0; i < pRc->maxValidRate - 1; i++) {
        rate = pRc->validRateIndex[i];
        if ((pRateTable->info[rate].dot11Rate >= 21) && 
            (!WLAN_RC_PHY_SGI(pRateTable->info[rate].phy)) && 
            (pRateTable->info[rate].baseIndex == 
            pRateTable->info[rate+1].baseIndex)) 
        {
            if ((rate+1) == pRc->validRateIndex[i+1]) {
                /* delete the LGI rate from the valid rate table */
                for (j = i; j<pRc->maxValidRate - 1; j++) {
                    pRc->validRateIndex[j] = pRc->validRateIndex[j+1];
                }
                pRc->validRateIndex[j] = 0;
                pRc->maxValidRate--;
            }
        }
    }
}

/* Access functions for validTxRateMask */

static inline void
rcInitValidTxRate(TX_RATE_CTRL *pRc)
{
    A_UINT8     i;

    for (i = 0; i < pRc->rateTableSize; i++) {
        pRc->validRateIndex[i] = 0;
    }
}


static INLINE int
rcIsValidTxRate(const RATE_TABLE_11N *pRateTable, TX_RATE_CTRL *pRc,
                     A_UINT8 txRate)
{
    A_UINT8     i;

    for (i = 0; i < pRc->maxValidRate; i++) {
        if (pRc->validRateIndex[i] == txRate) {
            return TRUE;
        }
    }
    return FALSE;
}

/* Return true only for single stream */

static int
rcIsValidPhyRate(A_UINT32 phy, A_UINT32 capflag, int ignoreCW)
{
    if (WLAN_RC_PHY_HT(phy) && !(capflag & WLAN_RC_HT_FLAG)) {
        return FALSE;
    }
    if (WLAN_RC_PHY_DS(phy) && !(capflag & WLAN_RC_DS_FLAG))  {
        return FALSE;
    }
    if (WLAN_RC_PHY_TS(phy) && !(capflag & WLAN_RC_TS_FLAG))  {
        return FALSE;
    }
    if (WLAN_RC_PHY_SGI(phy) && !(capflag & WLAN_RC_SGI_FLAG)) {
        return FALSE;
    }
    /* Bug 54801: Only check for 40 Phy & 20 Cap. Removed 20 Phy & 40 Cap check. */
    if (!ignoreCW && WLAN_RC_PHY_HT(phy)) {
        if (WLAN_RC_PHY_40(phy) && !(capflag & WLAN_RC_40_FLAG)) {
            return FALSE;
       	}
    }
    return TRUE;
}

/*
 * Initialize the Valid Rate Index from valid entries in Rate Table
 */
static inline A_UINT8
rcSibInitValidRates(struct atheros_node *pSib, const RATE_TABLE_11N *pRateTable, A_UINT32 capflag
, const MAX_RATES *maxRates)
{
    TX_RATE_CTRL    *pRc  = (TX_RATE_CTRL *)(pSib);
    A_UINT8         i, hi = 0;
    A_UINT32        valid;
    A_UINT32        maxRate = (capflag & WLAN_RC_40_FLAG)? 
                              maxRates->max_ht40_tx_rateKbps :
                              maxRates->max_ht20_tx_rateKbps;

    for (i = 0; i < pRateTable->rateCount; i++) {
        valid = getValidFlags(pSib, pRateTable, i);

        if (valid == TRUE)
        {
            A_UINT32 phy = pRateTable->info[i].phy;

            if (!rcIsValidPhyRate(phy, capflag, FALSE)
                || (maxRate && (pRateTable->info[i].rateKbps > maxRate))
                )
                continue;
            pRc->validPhyRateIndex[phy][pRc->validPhyRateCount[phy]] = i;
            pRc->validPhyRateCount[phy] += 1;
            hi = A_MAX(hi, i);
        }
    }
    return hi;
}

#endif /* _RATECTRL11N_H_ */
