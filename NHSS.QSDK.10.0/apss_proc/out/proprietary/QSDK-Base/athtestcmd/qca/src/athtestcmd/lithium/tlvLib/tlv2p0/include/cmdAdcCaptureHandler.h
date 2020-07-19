/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from ./input/cmdAdcCaptureHandler.s
#ifndef _CMDADCCAPTUREHANDLER_H_
#define _CMDADCCAPTUREHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct adccapture_parms {
    A_UINT8	phyId;
    A_UINT8	adcCompMode;
    A_UINT16	chainMask;
    A_UINT8	adcTriggerMode;
    A_UINT8	pad[3];
} __ATTRIB_PACK CMD_ADCCAPTURE_PARMS;

typedef struct adccapturersp_parms {
    A_UINT8	phyId;
    A_UINT8	status;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_ADCCAPTURERSP_PARMS;

typedef void (*ADCCAPTURE_OP_FUNC)(void *pParms);
typedef void (*ADCCAPTURERSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initADCCAPTUREOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL ADCCAPTUREOp(void *pParms);

void* initADCCAPTURERSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL ADCCAPTURERSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDADCCAPTUREHANDLER_H_
