/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

// This is an auto-generated file from input\cmdStickyHandler.s
#ifndef _CMDSTICKYHANDLER_H_
#define _CMDSTICKYHANDLER_H_

#if defined(__cplusplus) || defined(__cplusplus__)
extern "C" {
#endif

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif //WIN32 || WIN64

#define TLV2_STICKY_FLAG_OP_MASK			0x00000001	//bit 0: 0 = stick#define	TLV2_STICKY_FLAG_OP_SHIFT			0
#define	TLV2_STICKY_FLAG_OP_SHIFT			0
#define TLV2_STICKY_FLAG_OP_CLEAR			0
#define TLV2_STICKY_FLAG_OP_WRITE			1
#define TLV2_STICKY_FLAG_CLEAR_ALL_MASK		0x00000002	// = 1 -> cl#define TLV2_STICKY_FLAG_CLEAR_ALL_SHIFT	1
#define TLV2_STICKY_FLAG_CLEAR_ALL_SHIFT	1
#define TLV2_STICKY_FLAG_PREPOST_MASK		0x80000000	//bit 31: 0 = #define TLV2_STICKY_FLAG_PREPOST_SHIFT		31
#define TLV2_STICKY_FLAG_PREPOST_SHIFT		31

typedef struct sticky_parms {
    A_UINT32	regAddress;
    A_UINT32	regMask;
    A_UINT32	stickyFlags;
    A_UINT32	regValue;
} __ATTRIB_PACK CMD_STICKY_PARMS;

typedef void (*STICKY_OP_FUNC)(void *pParms);

// Exposed functions

void* initSTICKYOpParms(A_UINT8 *pParmsCommon, PARM_OFFSET_TBL *pParmsOffset, PARM_DICT *pParmDict);
A_BOOL STICKYOp(void *pParms);

#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif //WIN32 || WIN64


#if defined(__cplusplus) || defined(__cplusplus__)
}
#endif

#endif //_CMDSTICKYHANDLER_H_
