/*
 *  Copyright (c) 2010 Atheros Communications Inc.  All rights reserved.
 */
/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _ATH_NOTIFY_TX_BCN_H
#define _ATH_NOTIFY_TX_BCN_H

struct  ath_notify_tx_bcn;
typedef struct  ath_notify_tx_bcn *ath_notify_tx_bcn_t;

#if ATH_SUPPORT_P2P
    
void
ath_notify_tx_beacon_attach(struct ath_softc *sc);

void
ath_notify_tx_beacon_detach(struct ath_softc *sc);

void
ath_tx_bcn_notify(struct ath_softc *sc);

int
ath_reg_notify_tx_bcn(ath_dev_t dev, 
                      ath_notify_tx_beacon_function callback,
                      void *arg);

int
ath_dereg_notify_tx_bcn(ath_dev_t dev);

#else

/* Benign Functions */

#define ath_notify_tx_beacon_attach(sc)  /**/ 
#define ath_notify_tx_beacon_detach(sc)  /**/ 
#define ath_tx_bcn_notify(sc)            /**/ 

static INLINE int
ath_reg_notify_tx_bcn(ath_dev_t dev, 
                      ath_notify_tx_beacon_function callback,
                      void *arg)
{ 
  return -1; 
}

static INLINE int
ath_dereg_notify_tx_bcn(ath_dev_t dev)
{ 
  return -1; 
}

#endif  //ATH_SUPPORT_P2P

#endif  //_ATH_NOTIFY_TX_BCN_H

