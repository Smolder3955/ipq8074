/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include "fw_util.h"
inline void
htonlm(void *sptr, int len)
{ 
    int i = 0;
    unsigned int *dptr = (unsigned int*)sptr;
    unsigned int words = 0;
    /* length 0 is not allowed, minimum 4 bytes */
    if (len <= 0) len = 4;
    for(i=0; i<len/4; i++) {
        dptr[i] = htonl(dptr[i]);
    }
}
inline void
ntohlm(void *sptr, int len)
{ 
    int i = 0;
    unsigned int *dptr = (unsigned int*)sptr;
    unsigned int words = 0;
    /* length 0 is not allowed, minimum 4 bytes */
    if (len <= 0) len = 4;
    for(i=0; i<len/4; i++) {
        dptr[i] = ntohl(dptr[i]);
    }
}
int get_number (const char *str, int *nbr)
{
    if (!str || !nbr) {
        return -1;
    }
    if (strlen(str) > 2 && str[0]=='0' && str[1]=='x') {
        sscanf(str, "%x", nbr);
        return 0;
    };
    /* some one gone crazy about octal numbers*/
    if (strlen(str) > 2 && str[0]=='0' && str[1]=='0') {
        if(sscanf(str, "%o", nbr) < 0) return -1;
        return 0;
    };
    /* proably number is given as non hex, and decimal*/
    if(sscanf(str, "%d", nbr) < 0) return -1;
    return 0;
}
