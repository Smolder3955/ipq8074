/*
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _ATH_AR9000_TxBF_CAL_H_
#define _ATH_AR9300_TxBF_CAL_H_
#ifdef  ATH_SUPPORT_TxBF
#include "ah.h"

#define MAXMCSRATE  0x17    /*MCS 23 is max supported MCS rate*/
#define MAXONESTREAMRATE 0x07
#define HT_RATE     0x80    /* HT rate indicator */

#define ONE_MS   1

#define DEBUG_KA     0

#define NUM_OF_BW   2   /* two bandwidth used for mapping:20M,40M */
#define NUM_OF_Ng   3   /* three group define used for mapping :Ng=0,Ng=1,Ng=2*/

#define MAX_BITS_PER_SYMBOL 8

#define NUM_OF_CHAINMASK (1 << AH_MAX_CHAINS)

#define BITS_PER_BYTE       8
#define BITS_PER_COMPLEX_SYMBOL (2 * BITS_PER_SYMBOL)
#define BITS_PER_SYMBOL      10

#define MAX_STREAMS 3
#define MAX_PILOTS  6
#define EVM_MIN     -128

#define TONE_40M	114
#define TONE_20M	56
#define NUM_ITER	8

#define NB_PHIN		(10-1)
#define NB_CORIDC	(12-1)
#define NB_SIN		(8-1)
#define	NB_PH		5
#define	NB_PSI		4
#define	NUM_ITER_V	6

#define NB_MHINV	13
#define EVM_TH		10
#define RC_MAX		6000
#define RC_MIN		100

#define BW_40M      1
#define BW_20M_LOW  2
#define BW_20M_UP   3


/* MIMO control field related bit definiton */

#define AR_nc_idx       0x0003  /* Nc Index*/
#define AR_nc_idx_S     0
#define AR_nr_idx       0x000c  /* Nr Index*/
#define AR_nr_idx_S     2
#define AR_bandwith     0x0010  /* MIMO Control Channel Width*/
#define AR_bandwith_S   4
#define AR_ng           0x0060  /* Grouping*/
#define AR_ng_S         5
#define AR_nb           0x0180  /* Coeffiecient Size*/
#define AR_nb_S         7
#define AR_CI           0x0600  /* Codebook Information*/
#define AR_CI_S         9
#define CAL_GAIN        0
#define SMOOTH          1

/* constant for TXBF IE */
#define ALL_GROUP       3
#define NO_CALIBRATION  0
#define INIT_RESP_CAL   3

/* constant for rate code */
#define MIN_THREE_STREAM_RATE   0x90
#define MIN_TWO_STREAM_RATE     0x88
#define MIN_HT_RATE             0x80

typedef struct 
{
	int32_t real;
	int32_t imag;
}COMPLEX;

bool Ka_Calculation(struct ath_hal *ah,int8_t Ntx_A,int8_t Nrx_A,int8_t Ntx_B,int8_t Nrx_B,
                    COMPLEX (*Hba_eff)[3][TONE_40M],COMPLEX (*H_eff_quan)[3][TONE_40M],u_int8_t *M_H
                    ,COMPLEX (*Ka)[TONE_40M],u_int8_t NESSA,u_int8_t NESSB,int8_t BW);

void
compress_v(COMPLEX (*V)[3],u_int8_t Nr,u_int8_t Nc,u_int8_t *phi,u_int8_t *psi);

void
H_to_CSI(struct ath_hal *ah,u_int8_t Ntx,u_int8_t Nrx,COMPLEX (*H)[3][TONE_40M],u_int8_t *M_H,u_int8_t Ntone);
void
ar9300_get_local_h(struct ath_hal *ah, u_int8_t *local_h,int local_h_length,int Ntone,
                COMPLEX (*output_h) [3][TONE_40M]);
                
#endif
#endif
