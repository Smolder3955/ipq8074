/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */


#define TLV2_STICKY_FLAG_OP_MASK			0x00000001	//bit 0: 0 = sticky clear; 1 = sticky write (default)
#define	TLV2_STICKY_FLAG_OP_SHIFT			0
#define TLV2_STICKY_FLAG_OP_CLEAR			0
#define TLV2_STICKY_FLAG_OP_WRITE			1

#define TLV2_STICKY_FLAG_CLEAR_ALL_MASK		0x00000002	// = 1 -> clear all the sticky writes if bit 0 is 0
#define TLV2_STICKY_FLAG_CLEAR_ALL_SHIFT	1

#define TLV2_STICKY_FLAG_PREPOST_MASK		0x80000000	//bit 31: 0 = pre; 1 = post (default)
#define TLV2_STICKY_FLAG_PREPOST_SHIFT		31

# cmd
CMD= Sticky

# cmd parm
PARM_START:
UINT32:regAddress:1:x:0
UINT32:regMask:1:x:0xffffffff
UINT32:stickyFlags:1:x:0x80000001
UINT32:regValue:1:x:0
PARM_END:
