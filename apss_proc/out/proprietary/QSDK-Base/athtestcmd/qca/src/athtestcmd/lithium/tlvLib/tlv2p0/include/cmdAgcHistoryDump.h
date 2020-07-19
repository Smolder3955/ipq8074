/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdAgcHistoryDump.s
#ifndef _CMDAGCHISTORYDUMP_H_
#define _CMDAGCHISTORYDUMP_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct agchistorydump_parms {
    A_UINT32	AGCHistorydumpOption;
    A_UINT8	phyId;
    A_UINT8	pad[3];
} __ATTRIB_PACK CMD_AGCHISTORYDUMP_PARMS;

typedef struct agchistorydumprsp_parms {
    A_UINT8	AGCHistoryData[320];
    A_UINT8	status;
    A_UINT8	phyId;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_AGCHISTORYDUMPRSP_PARMS;

typedef void (*AGCHISTORYDUMP_OP_FUNC)(void *pParms);
typedef void (*AGCHISTORYDUMPRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initAGCHISTORYDUMPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL AGCHISTORYDUMPOp(void *pParms);

void* initAGCHISTORYDUMPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL AGCHISTORYDUMPRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDAGCHISTORYDUMP_H_
