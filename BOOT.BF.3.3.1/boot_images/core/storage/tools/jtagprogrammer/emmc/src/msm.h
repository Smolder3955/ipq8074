#ifndef __MSM_H__
#define __MSM_H__
/*
===========================================================================
*/
/**
  @file msm.h 

  Consolidated header file for all target specific public HWIO macro
  definitions.

  This file encapsulates all the MSM hardware specific header files,
  including those for HWIO register access.  It defines no interfaces
  itself but simply pulls in all required header files.
*/
/*  
  ====================================================================

  Copyright (c) 2017 Qualcomm Technologies, Inc.
  Copyright (c) 2010 Qualcomm Technologies Incorporated.  All Rights Reserved.  
  Qualcomm Technologies, Inc. Confidential and Proprietary.

  ==================================================================== 
  $Header: //components/rel/boot.bf/3.3.1/boot_images/core/storage/tools/jtagprogrammer/emmc/src/msm.h#2 $ $DateTime: 2017/04/11 22:02:31 $ $Author: pwbldsvc $
  ====================================================================
*/ 

/*=========================================================================
      Include Files
==========================================================================*/

#ifdef __cplusplus
extern "C"
{
#endif


/*
 * Common types.
 */
#ifndef _ARM_ASM_
#include "comdef.h"
#endif

/*
 * Public register definitions
 */
//#include "msmhwioreg.h"

/*
 * Global shadow register definitions.
 */
#ifndef _ARM_ASM_
//#include "msmshadow.h"
#endif

/*
 * HWIO access macros.
 */
#include "msmhwio.h"

/*
 * Bit shifting assist macros.
 */
//#include "msmshift.h"


/*
 * For now include private definitions here until drivers move to
 * include msmp.h directly.
 */
#ifdef FEATURE_LIBRARY_ONLY
#include "msmp.h"
#endif


#ifdef __cplusplus
}
#endif    


#endif /* __MSM_H__ */

