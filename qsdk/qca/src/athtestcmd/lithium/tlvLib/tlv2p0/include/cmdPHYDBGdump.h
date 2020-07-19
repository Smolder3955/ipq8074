/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input/cmdPHYDBGdump.s
#ifndef _CMDPHYDBGDUMP_H_
#define _CMDPHYDBGDUMP_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

typedef struct phydbgdump_parms {
    A_UINT32	PHYDBGdumpOption;
    A_UINT8	phyId;
    A_UINT8	pad[3];
} __ATTRIB_PACK CMD_PHYDBGDUMP_PARMS;

typedef struct phydbgdumprsp_parms {
    A_UINT8	data4k[4096];
    A_UINT8	status;
    A_UINT8	phyId;
    A_UINT8	pad[2];
} __ATTRIB_PACK CMD_PHYDBGDUMPRSP_PARMS;

typedef void (*PHYDBGDUMP_OP_FUNC)(void *pParms);
typedef void (*PHYDBGDUMPRSP_OP_FUNC)(void *pParms);

// Exposed functions

void* initPHYDBGDUMPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL PHYDBGDUMPOp(void *pParms);

void* initPHYDBGDUMPRSPOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL PHYDBGDUMPRSPOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDPHYDBGDUMP_H_
