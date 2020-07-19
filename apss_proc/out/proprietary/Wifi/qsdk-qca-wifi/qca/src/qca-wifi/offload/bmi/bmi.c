//------------------------------------------------------------------------------
// <copyright file="bmi.c" company="Atheros">
//    Copyright (c) 2004-2010 Atheros Corporation.  All rights reserved.
// $ATH_LICENSE_HOSTSDK0_C$
//------------------------------------------------------------------------------
//==============================================================================
//
// Author(s): ="Atheros"
//==============================================================================
/*
 * Copyright (c) 2014,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *  2014 Qualcomm Atheros, Inc.  All rights reserved.
 *
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners.
 */

#ifdef WIN_MOBILE7
#include <ntddk.h>
#endif
#include <hif.h>
#include "bmi.h"
#include "bmi_msg.h"
#include "bmi_internal.h"

#ifdef HIF_MESSAGE_BASED

#ifdef HIF_USB
#include "a_usb_defs.h"
#endif

#if defined(HIF_BMI_MAX_TRANSFER_SIZE)

#if BMI_DATASZ_MAX > HIF_BMI_MAX_TRANSFER_SIZE
    /* override */
#undef BMI_DATASZ_MAX
#define BMI_DATASZ_MAX  HIF_BMI_MAX_TRANSFER_SIZE
#endif
#endif

#endif

#ifdef DEBUG
static ATH_DEBUG_MASK_DESCRIPTION bmi_debug_desc[] = {
    { ATH_DEBUG_BMI , "BMI Tracing"},
};

ATH_DEBUG_INSTANTIATE_MODULE_VAR(bmi,
                                 "bmi",
                                 "Boot Manager Interface",
                                 ATH_DEBUG_MASK_DEFAULTS,
                                 ATH_DEBUG_DESCRIPTION_COUNT(bmi_debug_desc),
                                 bmi_debug_desc);

#endif

/*
Although we had envisioned BMI to run on top of HTC, this is not how the
final implementation ended up. On the Target side, BMI is a part of the BSP
and does not use the HTC protocol nor even DMA -- it is intentionally kept
very simple.
*/

#define MAX_BMI_CMDBUF_SZ (BMI_DATASZ_MAX + \
                       sizeof(A_UINT32) /* cmd */ + \
                       sizeof(A_UINT32) /* addr */ + \
                       sizeof(A_UINT32))/* length */
#define BMI_COMMAND_FITS(sz) ((sz) <= MAX_BMI_CMDBUF_SZ)
#define BMI_EXCHANGE_TIMEOUT_MS  1000

/* APIs visible to the driver */

struct bmi_info *BMIAlloc()
{
    struct bmi_info *bmi_handle;

    bmi_handle =(struct bmi_info*)qdf_mem_malloc(sizeof(struct bmi_info));
    if(!bmi_handle)
	return NULL;

    bmi_handle->pBMICmdBuf=NULL;
    bmi_handle->pBMIRspBuf=NULL;

    return bmi_handle;
}
qdf_export_symbol(BMIAlloc);

void BMIFree(struct bmi_info * bmi_handle)
{
   qdf_mem_free(bmi_handle);
}
qdf_export_symbol(BMIFree);

void
BMIInit(struct bmi_info *bmi_handle,void * sc_osdev)
{
    bmi_handle->bmiDone = FALSE;

     /*
     * On some platforms, it's not possible to DMA to a static variable
     * in a device driver (e.g. Linux loadable driver module).
     * So we need to A_MALLOC space for "command credits" and for commands.
     *
     * Note: implicitly relies on A_MALLOC to provide a buffer that is
     * suitable for DMA (or PIO).  This buffer will be passed down the
     * bus stack.
     */

    if (!bmi_handle->pBMICmdBuf) {
        bmi_handle->pBMICmdBuf =
                (A_UCHAR *)OS_MALLOC_CONSISTENT((osdev_t)sc_osdev,
                                    MAX_BMI_CMDBUF_SZ,
                                    &bmi_handle->BMICmd_pa,
                                    OS_GET_DMA_MEM_CONTEXT(bmi_handle,
                                    bmicmd_dmacontext), 0);
         ASSERT(bmi_handle->pBMICmdBuf);
    }

    if (!bmi_handle->pBMIRspBuf) {
        bmi_handle->pBMIRspBuf =
                (A_UCHAR *)OS_MALLOC_CONSISTENT((osdev_t)sc_osdev,
                                MAX_BMI_CMDBUF_SZ,
                                &bmi_handle->BMIRsp_pa,
                                OS_GET_DMA_MEM_CONTEXT(bmi_handle, bmirsp_dmacontext), 0);
         ASSERT(bmi_handle->pBMIRspBuf);
    }

    A_REGISTER_MODULE_DEBUG_INFO(bmi);
    bmi_handle->bmi_cleanup = &BMICleanup;
    bmi_handle->bmi_done = &BMIDone;
    bmi_handle->bmi_free = &BMIFree;

}
qdf_export_symbol(BMIInit);

void
BMICleanup(struct bmi_info *bmi_handle,void *sc_osdev)
{
    if (bmi_handle->pBMICmdBuf) {
        OS_FREE_CONSISTENT((osdev_t)sc_osdev, MAX_BMI_CMDBUF_SZ,
                        bmi_handle->pBMICmdBuf, bmi_handle->BMICmd_pa,
                        OS_GET_DMA_MEM_CONTEXT(bmi_handle, bmicmd_dmacontext));
        bmi_handle->pBMICmdBuf = NULL;
        bmi_handle->BMICmd_pa = 0;
    }

    if (bmi_handle->pBMIRspBuf) {
        OS_FREE_CONSISTENT((osdev_t)sc_osdev, MAX_BMI_CMDBUF_SZ,
                          bmi_handle->pBMIRspBuf, bmi_handle->BMIRsp_pa,
                          OS_GET_DMA_MEM_CONTEXT(bmi_handle, bmirsp_dmacontext));
        bmi_handle->pBMIRspBuf = NULL;
        bmi_handle->BMIRsp_pa = 0;
    }
}
qdf_export_symbol(BMICleanup);

A_STATUS
BMIDone(struct hif_opaque_softc *device, struct bmi_info *bmi_handle,void * sc_osdev)
{
    A_STATUS status;
    A_UINT32 cid;

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF (ATH_DEBUG_BMI, ("BMIDone skipped\n"));
        return A_OK;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Done: Enter (device: 0x%pK)\n", device));

#if defined(A_SIMOS_DEVHOST)
    /* Let HIF layer know that BMI phase is done.
     * Note that this call enqueues a bunch of receive buffers,
     * so it is important that this complete before we tell the
     * target about BMI_DONE.
     */
    (void)HIFConfigureDevice(device, HIF_BMI_DONE, NULL, 0);
#endif

    bmi_handle->bmiDone = TRUE;
    cid = BMI_DONE;
    if (!bmi_handle->pBMICmdBuf) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("pBMICmdBuf is NULL\n"));
        return A_ERROR;
    }
    A_MEMCPY(bmi_handle->pBMICmdBuf,&cid,sizeof(cid));

    status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                        bmi_handle->BMIRsp_pa, bmi_handle->pBMICmdBuf,
                        sizeof(cid), NULL, NULL, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
        return A_ERROR;
    }

    if (bmi_handle->pBMICmdBuf) {
        OS_FREE_CONSISTENT((osdev_t)sc_osdev, MAX_BMI_CMDBUF_SZ,
                        bmi_handle->pBMICmdBuf, bmi_handle->BMICmd_pa,
                        OS_GET_DMA_MEM_CONTEXT(bmi_handle, bmicmd_dmacontext));
        bmi_handle->pBMICmdBuf = NULL;
        bmi_handle->BMICmd_pa = 0;
    }

    if (bmi_handle->pBMIRspBuf) {
        OS_FREE_CONSISTENT((osdev_t)sc_osdev, MAX_BMI_CMDBUF_SZ,
                        bmi_handle->pBMIRspBuf,bmi_handle->BMIRsp_pa, OS_GET_DMA_MEM_CONTEXT(bmi_handle, bmirsp_dmacontext));
        bmi_handle->pBMIRspBuf = NULL;
        bmi_handle->BMIRsp_pa = 0;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Done: Exit\n"));

    return A_OK;
}
qdf_export_symbol(BMIDone);

#ifndef HIF_MESSAGE_BASED
extern A_STATUS HIFRegBasedGetTargetInfo(struct hif_opaque_softc *device, struct bmi_target_info *targ_info);
#endif

A_STATUS
BMIGetTargetInfo(struct hif_opaque_softc *device, void *targ_info,
                                      struct bmi_info *bmi_handle)
{
#ifndef HIF_MESSAGE_BASED
#else
    A_STATUS status;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = bmi_handle->pBMIRspBuf;
#endif

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("BMI Get Target Info Command disallowed\n"));
        return A_ERROR;
    }

#ifndef HIF_MESSAGE_BASED
        /* getting the target ID requires special handling because of the variable length
         * message */
    return HIFRegBasedGetTargetInfo(device,targ_info);
#else

    {
        A_UINT32 cid;
        A_UINT32 length;

        cid = BMI_GET_TARGET_INFO;
        A_MEMCPY(pBMICmdBuf,&cid,sizeof(cid));
        length = sizeof(struct bmi_target_info);

        status = hif_exchange_bmi_msg(device,
                                 bmi_handle->BMICmd_pa,
                                 bmi_handle->BMIRsp_pa,
                                 pBMICmdBuf,
                                 sizeof(cid),
                                 (A_UINT8 *)pBMIRspBuf,
                                 &length,
                                 BMI_EXCHANGE_TIMEOUT_MS);

        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to get target information from the device\n"));
            return A_ERROR;
        }

        A_MEMCPY((struct bmi_target_info *) targ_info, pBMIRspBuf, length);
        return status;
    }
#endif
}
qdf_export_symbol(BMIGetTargetInfo);

A_STATUS
BMIReadMemory(struct hif_opaque_softc *device,
              A_UINT32 address,
              A_UCHAR *buffer,
              A_UINT32 length,
               struct bmi_info *bmi_handle)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 remaining, rxlen;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = bmi_handle->pBMIRspBuf;
    A_UINT32 align;

    ASSERT(BMI_COMMAND_FITS(BMI_DATASZ_MAX + sizeof(cid) + sizeof(address) + sizeof(length)));
    memset (pBMICmdBuf, 0, BMI_DATASZ_MAX + sizeof(cid) + sizeof(address) + sizeof(length));
    memset (pBMIRspBuf, 0, BMI_DATASZ_MAX + sizeof(cid) + sizeof(address) + sizeof(length));

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
                ("BMI Read Memory: Enter (device: 0x%pK, address: 0x%x, length: %d)\n",
                    device, address, length));

    cid = BMI_READ_MEMORY;
#if defined(SDIO_3_0)
    /* 4bytes align operation */
    align = 4 - (length & 3);
    remaining = length + align;
#else
    align = 0;
    remaining = length;
#endif
    while (remaining)
    {
        rxlen = (remaining < BMI_DATASZ_MAX) ? remaining : BMI_DATASZ_MAX;
        offset = 0;
        A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
        offset += sizeof(cid);
        A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
        offset += sizeof(address);
        A_MEMCPY(&(pBMICmdBuf[offset]), &rxlen, sizeof(rxlen));
        offset += sizeof(length);

        status = hif_exchange_bmi_msg(device,
                                   bmi_handle->BMICmd_pa,
                                   bmi_handle->BMIRsp_pa,
                                   pBMICmdBuf,
                                   offset,
                                   pBMIRspBuf, /* note we reuse the same buffer to receive on */
                                   &rxlen,
                                   BMI_EXCHANGE_TIMEOUT_MS);

        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to read from the device\n"));
            return A_ERROR;
        }
	if (remaining == rxlen) {
            A_MEMCPY(&buffer[length - remaining + align], pBMIRspBuf, rxlen - align); /* last align bytes are invalid */
	} else {
            A_MEMCPY(&buffer[length - remaining + align], pBMIRspBuf, rxlen);
	}
        remaining -= rxlen; address += rxlen;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Read Memory: Exit\n"));
    return A_OK;
}
qdf_export_symbol(BMIReadMemory);

A_STATUS
BMIWriteMemory(struct hif_opaque_softc *device,
               A_UINT32 address,
               A_UCHAR *buffer,
               A_UINT32 length,
                struct bmi_info *bmi_handle)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 remaining, txlen;
    const A_UINT32 header = sizeof(cid) + sizeof(address) + sizeof(length);
    A_UCHAR alignedBuffer[BMI_DATASZ_MAX];
    A_UCHAR *src;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;
    ASSERT(BMI_COMMAND_FITS(BMI_DATASZ_MAX + header));
    memset (pBMICmdBuf, 0, BMI_DATASZ_MAX + header);

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI Write Memory: Enter (device: 0x%pK, address: 0x%x, length: %d)\n",
         device, address, length));

    cid = BMI_WRITE_MEMORY;

    remaining = length;
    while (remaining)
    {
        src = &buffer[length - remaining];
        if (remaining < (BMI_DATASZ_MAX - header)) {
            if (remaining & 3) {
                /* align it with 4 bytes */
                remaining = remaining + (4 - (remaining & 3));
                memcpy(alignedBuffer, src, remaining);
                src = alignedBuffer;
            }
            txlen = remaining;
        } else {
            txlen = (BMI_DATASZ_MAX - header);
        }
        offset = 0;
        A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
        offset += sizeof(cid);
        A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
        offset += sizeof(address);
        A_MEMCPY(&(pBMICmdBuf[offset]), &txlen, sizeof(txlen));
        offset += sizeof(txlen);
        A_MEMCPY(&(pBMICmdBuf[offset]), src, txlen);
        offset += txlen;
        status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                        bmi_handle->BMIRsp_pa, pBMICmdBuf, offset, NULL, NULL,
                        BMI_EXCHANGE_TIMEOUT_MS);
        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
            return A_ERROR;
        }
        remaining -= txlen; address += txlen;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Write Memory: Exit\n"));

    return A_OK;
}
qdf_export_symbol(BMIWriteMemory);

A_STATUS
BMIExecute(struct hif_opaque_softc *device,
           A_UINT32 address,
           A_UINT32 *param,
            struct bmi_info *bmi_handle,
           A_UINT32 wait)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 paramLen;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = bmi_handle->pBMIRspBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address) + sizeof(param)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address) + sizeof(param));
    memset (pBMIRspBuf, 0, sizeof(cid) + sizeof(address) + sizeof(param));

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
       ("BMI Execute: Enter (device: 0x%pK, address: 0x%x, param: %d)\n",
        device, address, *param));

    cid = BMI_EXECUTE;

    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);
    A_MEMCPY(&(pBMICmdBuf[offset]), param, sizeof(*param));
    offset += sizeof(*param);
    paramLen = sizeof(*param);
    if (wait)
        status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                        bmi_handle->BMIRsp_pa, pBMICmdBuf, offset, pBMIRspBuf,
                        &paramLen, 0);
    else
        status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                        bmi_handle->BMIRsp_pa, pBMICmdBuf, offset, NULL,
                        NULL, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to read from the device\n"));
        return A_ERROR;
    }

    A_MEMCPY(param, pBMIRspBuf, sizeof(*param));

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Execute: Exit (param: %d)\n", *param));
    return A_OK;
}
qdf_export_symbol(BMIExecute);

A_STATUS
BMISetAppStart(struct hif_opaque_softc *device,
               A_UINT32 address,
                struct bmi_info *bmi_handle)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address));

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
       ("BMI Set App Start: Enter (device: 0x%pK, address: 0x%x)\n",
        device, address));

    cid = BMI_SET_APP_START;

    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);

    status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                    bmi_handle->BMIRsp_pa, pBMICmdBuf, offset, NULL, NULL, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Set App Start: Exit\n"));
    return A_OK;
}

A_STATUS
BMIReadSOCRegister(struct hif_opaque_softc *device,
                   A_UINT32 address,
                   A_UINT32 *param,
                    struct bmi_info *bmi_handle)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset,paramLen;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = bmi_handle->pBMIRspBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address));
    memset (pBMIRspBuf, 0, sizeof(cid) + sizeof(address));

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
       ("BMI Read SOC Register: Enter (device: 0x%pK, address: 0x%x)\n",
       device, address));

    cid = BMI_READ_SOC_REGISTER;

    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);
    paramLen = sizeof(*param);
    status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                     bmi_handle->BMIRsp_pa, pBMICmdBuf, offset, pBMIRspBuf,
                     &paramLen, BMI_EXCHANGE_TIMEOUT_MS);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to read from the device\n"));
        return A_ERROR;
    }
    A_MEMCPY(param, pBMIRspBuf, sizeof(*param));

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Read SOC Register: Exit (value: %d)\n", *param));
    return A_OK;
}

A_STATUS
BMIWriteSOCRegister(struct hif_opaque_softc *device,
                    A_UINT32 address,
                    A_UINT32 param,
                     struct bmi_info *bmi_handle)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address) + sizeof(param)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address) + sizeof(param));

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
     ("BMI Write SOC Register: Enter (device: 0x%pK, address: 0x%x, param: %d)\n",
     device, address, param));

    cid = BMI_WRITE_SOC_REGISTER;

    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);
    A_MEMCPY(&(pBMICmdBuf[offset]), &param, sizeof(param));
    offset += sizeof(param);
    status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                     bmi_handle->BMIRsp_pa, pBMICmdBuf, offset, NULL, NULL, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI Read SOC Register: Exit\n"));
    return A_OK;
}

A_STATUS
BMILZData(struct hif_opaque_softc *device,
          A_UCHAR *buffer,
          A_UINT32 length,
          struct bmi_info *bmi_handle)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 remaining, txlen;
    const A_UINT32 header = sizeof(cid) + sizeof(length);
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;

    ASSERT(BMI_COMMAND_FITS(BMI_DATASZ_MAX+header));
    memset (pBMICmdBuf, 0, BMI_DATASZ_MAX+header);

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI Send LZ Data: Enter (device: 0x%pK, length: %d)\n",
         device, length));

    cid = BMI_LZ_DATA;

    remaining = length;
    while (remaining)
    {
        txlen = (remaining < (BMI_DATASZ_MAX - header)) ?
                                       remaining : (BMI_DATASZ_MAX - header);
        offset = 0;
        A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
        offset += sizeof(cid);
        A_MEMCPY(&(pBMICmdBuf[offset]), &txlen, sizeof(txlen));
        offset += sizeof(txlen);
        A_MEMCPY(&(pBMICmdBuf[offset]), &buffer[length - remaining], txlen);
        offset += txlen;
        status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                        bmi_handle->BMIRsp_pa, pBMICmdBuf, offset,
                        NULL, NULL, 0);
        if (status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
            return A_ERROR;
        }
        remaining -= txlen;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI LZ Data: Exit\n"));

    return A_OK;
}

A_STATUS
BMISignStreamStart(struct hif_opaque_softc *device,
                   A_UINT32 address,
                   A_UCHAR *buffer,
                   A_UINT32 length,
                   struct bmi_info *bmi_handle)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    const A_UINT32 header = sizeof(cid) + sizeof(address) + sizeof(length);
    A_UCHAR alignedBuffer[BMI_DATASZ_MAX + 4];
    A_UCHAR *src;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;
    A_UINT32 remaining, txlen;

    ASSERT(BMI_COMMAND_FITS(BMI_DATASZ_MAX + header));
    memset(pBMICmdBuf, 0, BMI_DATASZ_MAX + header);

    if(bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI SIGN Stream Start: Enter (device: 0x%pK, address: 0x%x, length: %d)\n",
         device, address, length));

    cid = BMI_SIGN_STREAM_START;
    remaining = length;
    while(remaining)
    {
        src = &buffer[length - remaining];
        if(remaining < (BMI_DATASZ_MAX - header)) {
            /*  Actually it shall be aligned binary from header definition.
             *  Not necessary for align process. Kept for possible changes */
            if(remaining & 0x3) {
                remaining = remaining + (4 - (remaining & 0x3));
                memcpy(alignedBuffer, src, remaining);
                src = alignedBuffer;
            }
            txlen = remaining;
        } else {
            txlen = (BMI_DATASZ_MAX - header);
        }

        offset = 0;
        A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
        offset += sizeof(cid);
        A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
        offset += sizeof(offset);
        A_MEMCPY(&(pBMICmdBuf[offset]), &txlen, sizeof(txlen));
        offset += sizeof(txlen);
        A_MEMCPY(&(pBMICmdBuf[offset]), src, txlen);
        offset += txlen;
        status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                        bmi_handle->BMIRsp_pa, pBMICmdBuf, offset, NULL,
                        NULL, BMI_EXCHANGE_TIMEOUT_MS);
        if(status != A_OK) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to write to the device\n"));
            return A_ERROR;
        }
        remaining -= txlen;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI SIGN Stream Start: Exit\n"));

    return A_OK;
}

A_STATUS
BMILZStreamStart(struct hif_opaque_softc *device,
                 A_UINT32 address,
                 struct bmi_info *bmi_handle)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + sizeof(address)));
    memset (pBMICmdBuf, 0, sizeof(cid) + sizeof(address));

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI LZ Stream Start: Enter (device: 0x%pK, address: 0x%x)\n",
         device, address));

    cid = BMI_LZ_STREAM_START;
    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), &address, sizeof(address));
    offset += sizeof(address);
    status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                     bmi_handle->BMIRsp_pa, pBMICmdBuf, offset, NULL, NULL, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to Start LZ Stream to the device\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI LZ Stream Start: Exit\n"));

    return A_OK;
}

A_STATUS
BMIFastDownload(struct hif_opaque_softc *device, A_UINT32 address, A_UCHAR *buffer, A_UINT32 length, struct bmi_info *bmi_handle)
{
    A_STATUS status = A_ERROR;
    A_UINT32  lastWord = 0;
    A_UINT32  lastWordOffset = length & ~0x3;
    A_UINT32  unalignedBytes = length & 0x3;

    status = BMILZStreamStart (device, address, bmi_handle);
    if (A_FAILED(status)) {
            return A_ERROR;
    }

    if (unalignedBytes) {
            /* copy the last word into a zero padded buffer */
        A_MEMCPY(&lastWord, &buffer[lastWordOffset], unalignedBytes);
    }

    status = BMILZData(device, buffer, lastWordOffset,bmi_handle);

    if (A_FAILED(status)) {
        return A_ERROR;
    }

    if (unalignedBytes) {
        status = BMILZData(device, (A_UINT8 *)&lastWord, 4,bmi_handle);
    }

    if (A_SUCCESS(status)) {
        //
        // Close compressed stream and open a new (fake) one.  This serves mainly to flush Target caches.
        //
        status = BMILZStreamStart (device, 0x00, bmi_handle);
        if (A_FAILED(status)) {
           return A_ERROR;
        }
    }
    return status;
}
qdf_export_symbol(BMIFastDownload);

A_STATUS
BMInvramProcess(struct hif_opaque_softc *device, A_UCHAR *seg_name, A_UINT32 *retval,
                 struct bmi_info *bmi_handle)
{
    A_UINT32 cid;
    A_STATUS status;
    A_UINT32 offset;
    A_UINT32 retvalLen;
    A_UCHAR *pBMICmdBuf = bmi_handle->pBMICmdBuf;
    A_UCHAR *pBMIRspBuf = bmi_handle->pBMIRspBuf;

    ASSERT(BMI_COMMAND_FITS(sizeof(cid) + BMI_NVRAM_SEG_NAME_SZ));

    if (bmi_handle->bmiDone) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Command disallowed\n"));
        return A_ERROR;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI,
         ("BMI NVRAM Process: Enter (device: 0x%pK, name: %s)\n",
           device, seg_name));

    cid = BMI_NVRAM_PROCESS;
    offset = 0;
    A_MEMCPY(&(pBMICmdBuf[offset]), &cid, sizeof(cid));
    offset += sizeof(cid);
    A_MEMCPY(&(pBMICmdBuf[offset]), seg_name, BMI_NVRAM_SEG_NAME_SZ);
    offset += BMI_NVRAM_SEG_NAME_SZ;
    retvalLen = sizeof(*retval);
    status = hif_exchange_bmi_msg(device, bmi_handle->BMICmd_pa,
                bmi_handle->BMIRsp_pa, pBMICmdBuf, offset, pBMIRspBuf,
                &retvalLen, 0);
    if (status != A_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Unable to access the device\n"));
        return A_ERROR;
    }

    A_MEMCPY(retval, pBMIRspBuf, sizeof(*retval));

    AR_DEBUG_PRINTF(ATH_DEBUG_BMI, ("BMI NVRAM Process: Exit\n"));

    return A_OK;
}

bool BMIIsCmdBmiDone(uint32_t cmd)
{
	return ((BMI_DONE == cmd) ? true:false);
}
qdf_export_symbol(BMIIsCmdBmiDone);

int32_t BMIIsCmdResponseExpected(uint32_t cmd)
{
    switch(cmd) {
    /* Commands that expect a response from the Target */
    case BMI_READ_MEMORY:
    case BMI_EXECUTE:
    case BMI_READ_SOC_WORD:
    case BMI_GET_TARGET_INFO:
    case BMI_ROMPATCH_INSTALL:
    case BMI_NVRAM_PROCESS:
	return true;

    /* Commands that do NOT expect a response from the Target */
    case BMI_WRITE_MEMORY:
    case BMI_SET_APP_START:
    case BMI_WRITE_SOC_WORD:
    case BMI_ROMPATCH_UNINSTALL:
    case BMI_ROMPATCH_ACTIVATE:
    case BMI_ROMPATCH_DEACTIVATE:
    case BMI_LZ_STREAM_START:
    case BMI_LZ_DATA:
        return false;

    default:
        qdf_print(KERN_ERR "BMI sysfs command unknown (%d)", cmd);
        return A_ERROR;
    }
}
qdf_export_symbol(BMIIsCmdResponseExpected);

uint32_t BMIGetMaxDataSize(void)
{
	return BMI_DATASZ_MAX;
}
qdf_export_symbol(BMIGetMaxDataSize);

#ifdef HIF_MESSAGE_BASED

/* TODO.. stubs.. for message-based HIFs, the RAW access APIs need to be changed
 */

A_STATUS
BMIRawWrite(struct hif_opaque_softc *device, A_UCHAR *buffer, A_UINT32 length)
{
        /* TODO */
    return A_ERROR;
}

A_STATUS
BMIRawRead(struct hif_opaque_softc *device, A_UCHAR *buffer, A_UINT32 length, A_BOOL want_timeout)
{
        /* TODO */
    return A_ERROR;
}

#endif


