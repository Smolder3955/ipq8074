/*
 * Copyright (c) 2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary
*/

#define ACFG_STR_MATCH(str1, str2)  (strcmp(((char *)str1), ((char *)str2)) == 0)
#define ACFG_N_STR_MATCH(str1, str2)  (strncmp(((char *)str1), ((char *)str2), strlen((char*)str2)) == 0)
#define ACFG_MAC_STR_LEN         17
#define ACFG_ENABLE_PARAM "enable"
#define ACFG_DISABLE_PARAM "disable"
#define ACFG_HTOLE16(X)  (((((uint16_t)(X)) << 8) | ((uint16_t)(X) >> 8)) & 0xffff)
#define ACFG_ROUNDUP(x, y)   ((((x)+((y)-1))/(y))*(y))  /* to any y */
