/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdLMRxStatusHandler.s
#ifndef _CMDLMRXSTATUSHANDLER_H_
#define _CMDLMRXSTATUSHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct lmrxstatus_parms {
    A_UINT8	lm_flag;
    A_UINT8	lm_itemNum;
    A_UINT8	pad2[2];
} __ATTRIB_PACK CMD_LMRXSTATUS_PARMS;

typedef struct lmrxstatusrsp_parms {
    A_UINT8	lm_phyId[100];
    A_UINT32	lm_totalGoodPackets[100];
    A_UINT32	lm_goodPackets[100];
    A_UINT32	lm_otherErrors[100];
    A_UINT32	lm_crcPackets[100];
    A_UINT32	lm_decrypErrors[100];
    A_UINT32	lm_rateBit[100];
    A_UINT32	lm_statTime[100];
    A_UINT32	lm_endTime[100];
    A_UINT32	lm_byteCount[100];
    A_UINT32	lm_dontCount[100];
    A_UINT32	lm_rssi[100];
} __ATTRIB_PACK CMD_LMRXSTATUSRSP_PARMS;

typedef void (*LMRXSTATUS_OP_FUNC)(void *pParms);
typedef void (*LMRXSTATUSRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initLMRXSTATUSOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL LMRXSTATUSOp(void *pParms);

void* initLMRXSTATUSRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL LMRXSTATUSRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDLMRXSTATUSHANDLER_H_
