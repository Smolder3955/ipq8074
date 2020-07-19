/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*
 * Definitions for the USB module
 */
#ifndef ATH_USB_H
#define ATH_USB_H

#ifdef ATH_USB

/* Important Note: When changing this definition please update the definition of
 * ATH_BCBUF in the file umac_lmac_common.h
 */
#define ATH_USB_BCBUF   8   /* number of beacon buffers */

#define ATH_USB_SET_INVALID(_sc, _val)      (_sc->sc_usb_invalid = _val)

#define ATH_USB_TQUEUE_INIT(_sc, _func)     (ATHUSB_INIT_TQUEUE(_sc->sc_osdev, &_sc->sc_osdev->rx_tq, _func, _sc))
#define ATH_USB_TQUEUE_FREE(_sc)            (ATHUSB_FREE_TQUEUE(_sc->sc_osdev, &_sc->sc_osdev->rx_tq))

#ifdef ATH_HTC_TX_SCHED
#define ATH_HTC_TXTQUEUE_INIT(_sc, _func)     (ATHHTC_INIT_TXTQUEUE(_sc->sc_osdev, &_sc->sc_osdev->htctx_tq, _func, _sc))
#define ATH_HTC_TXTQUEUE_FREE(_sc)            (ATHHTC_FREE_TXTQUEUE(_sc->sc_osdev, &_sc->sc_osdev->htctx_tq))
#define ATH_HTC_TXTQUEUE_SCHEDULE(_osdev)           (ATHHTC_SCHEDULE_TXTQUEUE(&_osdev->htctx_tq))
#define ATH_HTC_UAPSD_CREDITUPDATE_INIT(_sc, _func)  (ATHHTC_INIT_UAPSD_CREDITUPDATE(_sc->sc_osdev, \
                                                       &_sc->sc_osdev->htcuapsd_tq, _func, _sc))
#define ATH_HTC_UAPSD_CREDITUPDATE_FREE(_sc)   (ATHHTC_FREE_UAPSD_CREDITUPDATE(_sc->sc_osdev,&_sc->sc_osdev->htcuapsd_tq))
#define ATH_HTC_UAPSD_CREDITUPDATE_SCHEDULE(_osdev)      (ATHHTC_SCHEDULE_UAPSD_CREDITUPDATE(&_osdev->htcuapsd_tq))
#else
#define ATH_HTC_TXTQUEUE_INIT(_sc, _func)     
#define ATH_HTC_TXTQUEUE_FREE(_sc)           
#define ATH_HTC_TXTQUEUE_SCHEDULE(_osdev)   
#define ATH_HTC_UAPSD_CREDITUPDATE_INIT(_sc, _func)     
#define ATH_HTC_UAPSD_CREDITUPDATE_FREE(_sc)           
#define ATH_HTC_UAPSD_CREDITUPDATE_SCHEDULE(_osdev)
#endif
void ath_usb_suspend(ath_dev_t dev);
int ath_usb_set_tx(struct ath_softc *sc);
void ath_usb_rx_cleanup(ath_dev_t dev);
#define ATH_USB_TX_STOP(_osdev)    OS_Usb_Tx_Start_Stop(_osdev, AH_FALSE)
#define ATH_USB_TX_START(_osdev)    OS_Usb_Tx_Start_Stop(_osdev, AH_TRUE)


#else   /* ATH_USB */

#define ATH_USB_SET_INVALID(_sc, _val)

#define ATH_USB_TQUEUE_INIT(_sc, _func)
#define ATH_USB_TQUEUE_FREE(_sc)
#define ATH_HTC_TXTQUEUE_INIT(_sc, _func)     
#define ATH_HTC_TXTQUEUE_FREE(_sc)            
#define ATH_HTC_UAPSD_CREDITUPDATE_INIT(_sc, _func)     
#define ATH_HTC_UAPSD_CREDITUPDATE_FREE(_sc)           
#define ATH_HTC_UAPSD_CREDITUPDATE_SCHEDULE(_osdev)

#define ath_usb_suspend(dev)
#define ath_usb_set_tx(sc)                  (1)
//#define ath_usb_rx_cleanup(dev)
#define ATH_USB_TX_STOP(_osdev)
#define ATH_USB_TX_START(_osdev)
#endif  /* ATH_USB */

#endif /* ATH_USB_H */

