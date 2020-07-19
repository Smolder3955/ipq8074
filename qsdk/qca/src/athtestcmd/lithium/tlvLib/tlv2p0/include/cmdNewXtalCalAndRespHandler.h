/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdNewXtalCalAndRespHandler.s
#ifndef _CMDNEWXTALCALANDRESPHANDLER_H_
#define _CMDNEWXTALCALANDRESPHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct newxtalcalproc_parms {
    A_UINT16	newCapIn;
    A_UINT16	newCapOut;
    A_UINT8	ctrlFlag;
    A_UINT8	phyId;
    A_UINT8	band;
    A_UINT8	pad1;
} __ATTRIB_PACK CMD_NEWXTALCALPROC_PARMS;

typedef struct newxtalcalprocrsp_parms {
    A_UINT8	status;
    A_UINT8	pllLocked;
    A_UINT8	phyId;
    A_UINT8	band;
    A_UINT16	capInValMin;
    A_UINT16	capInValMax;
    A_UINT16	capOutValMin;
    A_UINT16	capOutValMax;
    A_UINT16	newCapIn;
    A_UINT16	newCapOut;
} __ATTRIB_PACK CMD_NEWXTALCALPROCRSP_PARMS;

typedef void (*NEWXTALCALPROC_OP_FUNC)(void *pParms);
typedef void (*NEWXTALCALPROCRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initNEWXTALCALPROCOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL NEWXTALCALPROCOp(void *pParms);

void* initNEWXTALCALPROCRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL NEWXTALCALPROCRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDNEWXTALCALANDRESPHANDLER_H_
