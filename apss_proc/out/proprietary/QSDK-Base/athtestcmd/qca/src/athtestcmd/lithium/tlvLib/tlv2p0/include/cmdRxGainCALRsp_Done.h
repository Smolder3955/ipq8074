/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdRxGainCALRsp_Done.s
#ifndef _CMDRXGAINCALRSP_DONE_H_
#define _CMDRXGAINCALRSP_DONE_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct rxgaincal_sigl_done_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	band;
    A_UINT8	pad1;
} __ATTRIB_PACK CMD_RXGAINCAL_SIGL_DONE_PARMS;

typedef struct rxgaincalrsp_done_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	band;
    A_UINT8	pad1;
} __ATTRIB_PACK CMD_RXGAINCALRSP_DONE_PARMS;

typedef void (*RXGAINCAL_SIGL_DONE_OP_FUNC)(void *pParms);
typedef void (*RXGAINCALRSP_DONE_OP_FUNC)(void *pParms);

// Exposed functions

void* initRXGAINCAL_SIGL_DONEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL RXGAINCAL_SIGL_DONEOp(void *pParms);

void* initRXGAINCALRSP_DONEOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL RXGAINCALRSP_DONEOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDRXGAINCALRSP_DONE_H_
