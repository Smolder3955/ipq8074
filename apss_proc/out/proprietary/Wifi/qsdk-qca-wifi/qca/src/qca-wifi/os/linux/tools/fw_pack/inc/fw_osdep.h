/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifdef __linux__
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>                     /* for basename */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* generic types*/
typedef unsigned int uint32;
typedef unsigned short int uint16;
typedef unsigned char uint8;
inline void htonlm(void *sptr, int len);
inline void ntohlm(void *sptr, int len);
#elif defined (WIN32)

#error "Windows defintions are not specified yet"
#endif
