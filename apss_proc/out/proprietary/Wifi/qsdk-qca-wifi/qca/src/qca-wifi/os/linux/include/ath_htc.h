/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 *  Copyright (c) 2008 Atheros Communications Inc.  All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* 
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved. 
 * Qualcomm Atheros Confidential and Proprietary. 
 */ 

#ifndef ATH_HTC_H
#define ATH_HTC_H


/* usbdrv.c */
int athLnxAllocAllUrbs(osdev_t osdev);
void athLnxFreeAllUrbs(osdev_t osdev);
void athLnxUnlinkAllUrbs(osdev_t osdev);
int athLnxRebootCmd(osdev_t osdev);
void athLnxStartPerSecTimer(osdev_t osdev);
void athLnxStopPerSecTimer(osdev_t osdev);
#ifdef NBUF_PREALLOC_POOL
int athLnxAllocRegInBuf(osdev_t osdev);
void athLnxFreeRegInBuf(osdev_t osdev);
#endif

/* ath_htc.c */
int MpHtcAttach(osdev_t osdev);
void MpHtcDetach(osdev_t osdev);
void MpWmiStop(osdev_t osdev);
void MpWmiStart(osdev_t osdev);
void MpHtcSuspend(osdev_t osdev);
void MpHtcResume(osdev_t osdev);

u_int32_t
athUsbFirmwareDownload(osdev_t osdev, uint8_t* fw, uint32_t len, uint32_t offset);

int athLnxCreateRecoverWorkQueue(osdev_t osdev);

//void MpSend_Usb_Reboot_Cmd(PNIC pNic, BOOLEAN initstate);

//extern void HtcInitTqueue(PADAPTER           pAdapter,                   
//                   PA_USBTASKLET      ptasklet, 
//                   PFN_USBTASKLET_CALLBACK  pfunc,
//                   void               *pcontext);
                   
//void HtcFreeTqueue(PADAPTER           pAdapter,                   
//                   PA_USBTASKLET      ptasklet);
//void MP_OS_INIT_TIMER(PNIC pNic, PVOID ss1, PVOID ss2, PVOID ss3);                   

#endif //#ifndef ATH_HTC_H
