/*
 * Copyright (c) 2013, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


//==============================================================================
// BMI declarations and prototypes
//
// Author(s): ="Atheros"
//==============================================================================
#ifndef _BMI_H_
#define _BMI_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Header files */
#include "athdefs.h"
#include "a_types.h"
#include "a_osapi.h"
#include "hif.h"
#include "ol_if_athvar.h"
#include "hif.h"

/**
 * struct bmi_info - holds BMI info
 * @bmi_ol_priv: OS-dependent private info for BMI
 * @bmiDone: flag represnts BMI status
 * @bmiUADone: BMI User agent status
 * @pBMICmdBuf: BMI Command buf
 * @BMICmd_pa: BMI command physical address
 * @bmicmd_dmacontext: BMI CMD DMA context
 * @pBMIRspBuf: BMI response buffer
 * @BMIRsp_pa: BMI response physical address
 * @last_rxlen: length of last response
 * @bmirsp_dmacontext: BMI Response DMA context
 */
struct bmi_info {
    void            *bmi_ol_priv;
    bool            bmiDone;
    bool            bmiUADone;
    u_int8_t        *pBMICmdBuf;
    dma_addr_t      BMICmd_pa;
    OS_DMA_MEM_CONTEXT(bmicmd_dmacontext)
    u_int8_t        *pBMIRspBuf;
    dma_addr_t      BMIRsp_pa;
    u_int32_t       last_rxlen;
    OS_DMA_MEM_CONTEXT(bmirsp_dmacontext);
    void (*bmi_cleanup)(struct bmi_info *bmi_handle,void * sc_osdev);
    void (*bmi_free)(struct bmi_info * bmi_handle);
    A_STATUS (*bmi_done)(struct hif_opaque_softc *device,
                         struct bmi_info *bmi_handle,void * sc_osdev);

};

struct bmi_info *BMIAlloc(void);

void BMIFree(struct bmi_info * bmi_handle);

void
BMIInit(struct bmi_info *bmi_handle,void * sc_osdev);

void
BMICleanup(struct bmi_info *bmi_handle,void * sc_osdev);

A_STATUS
BMIDone(struct hif_opaque_softc *device,struct bmi_info *bmi_handle,void * sc_osdev);

A_STATUS
BMIGetTargetInfo(struct hif_opaque_softc *device, void *targ_info, struct bmi_info *bmi_handle);

A_STATUS
BMIReadMemory(struct hif_opaque_softc *device,
              A_UINT32 address,
              A_UCHAR *buffer,
              A_UINT32 length,
              struct bmi_info *bmi_handle);

A_STATUS
BMIWriteMemory(struct hif_opaque_softc *device,
               A_UINT32 address,
               A_UCHAR *buffer,
               A_UINT32 length,
               struct bmi_info *bmi_handle);

A_STATUS
BMIExecute(struct hif_opaque_softc *device,
           A_UINT32 address,
           A_UINT32 *param,
           struct bmi_info *bmi_handle,
           A_UINT32 wait);

A_STATUS
BMISetAppStart(struct hif_opaque_softc *device,
               A_UINT32 address,
               struct bmi_info *bmi_handle);

A_STATUS
BMIReadSOCRegister(struct hif_opaque_softc *device,
                   A_UINT32 address,
                   A_UINT32 *param,
                   struct bmi_info *bmi_handle);

A_STATUS
BMIWriteSOCRegister(struct hif_opaque_softc *device,
                    A_UINT32 address,
                    A_UINT32 param,
                    struct bmi_info *bmi_handle);

A_STATUS
BMIrompatchInstall(struct hif_opaque_softc *device,
                   A_UINT32 ROM_addr,
                   A_UINT32 RAM_addr,
                   A_UINT32 nbytes,
                   A_UINT32 do_activate,
                   A_UINT32 *patch_id,
                   struct bmi_info *bmi_handle);

A_STATUS
BMIrompatchUninstall(struct hif_opaque_softc *device,
                     A_UINT32 rompatch_id,
                     struct bmi_info *bmi_handle);

A_STATUS
BMIrompatchActivate(struct hif_opaque_softc *device,
                    A_UINT32 rompatch_count,
                    A_UINT32 *rompatch_list,
                    struct bmi_info *bmi_handle);

A_STATUS
BMIrompatchDeactivate(struct hif_opaque_softc *device,
                      A_UINT32 rompatch_count,
                      A_UINT32 *rompatch_list,
                      struct bmi_info *bmi_handle);

A_STATUS
BMISignStreamStart(struct hif_opaque_softc *device,
                   A_UINT32 address,
                   A_UCHAR *buffer,
                   A_UINT32 length,
                   struct bmi_info *bmi_handle);

A_STATUS
BMILZStreamStart(struct hif_opaque_softc *device,
                 A_UINT32 address,
                 struct bmi_info *bmi_handle);

A_STATUS
BMILZData(struct hif_opaque_softc *device,
          A_UCHAR *buffer,
          A_UINT32 length,
          struct bmi_info *bmi_handle);

A_STATUS
BMIFastDownload(struct hif_opaque_softc *device,
                A_UINT32 address,
                A_UCHAR *buffer,
                A_UINT32 length,
                struct bmi_info *bmi_handle);

A_STATUS
BMInvramProcess(struct hif_opaque_softc *device,
                A_UCHAR *seg_name,
                A_UINT32 *retval,
                struct bmi_info *bmi_handle);

A_STATUS
BMIRawWrite(struct hif_opaque_softc *device,
            A_UCHAR *buffer,
            A_UINT32 length);

A_STATUS
BMIRawRead(struct hif_opaque_softc *device,
           A_UCHAR *buffer,
           A_UINT32 length,
           A_BOOL want_timeout);

bool BMIIsCmdBmiDone(uint32_t cmd);
int32_t BMIIsCmdResponseExpected(uint32_t cmd);
uint32_t BMIGetMaxDataSize(void);

#ifdef __cplusplus
}
#endif

#endif /* _BMI_H_ */
