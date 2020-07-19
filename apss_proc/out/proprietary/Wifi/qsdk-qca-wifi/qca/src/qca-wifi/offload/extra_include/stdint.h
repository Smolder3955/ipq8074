/*
 * Copyright (c) 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */



/*
 * gcc 4.8 includes a non standard include directive #include_next.
 * This ignores to include the stanard headers and try to find next
 * available stdint.h header.
 * For lithium hardware generated header file includes the following
 * types which are available in stdint.h but, include_next makes it
 * not useful. GCC compiler option -ffreestanding would get __STD_HOSTED__
 * flag would disable #include_next, but that fails in rest of the
 * compilation due to issues of redefintion of macros SIZE_MAX. Unfortunately
 * this macro conflicts with two other kernel header files, kernel.h and
 * list.h, and both can't be modified. Instead of touching any of the
 * files, making changes to include extra header files, and making
 * necessary change in Kbuild seems good.
 */
#ifndef __user_stdint_not_defined_h
#define __user_stdint_not_defined_h 1 
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int32_t  int32;
typedef int16_t  int16;
typedef uint8_t  uint8;
typedef int8_t   int8;
typedef int32_t  int32;
#endif
